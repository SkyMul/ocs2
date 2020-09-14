/******************************************************************************
Copyright (c) 2017, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

 * Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

//
// Created by rgrandia on 26.02.20.
//

#include "ocs2_qp_solver/QpDiscreteTranscription.h"

namespace ocs2 {
namespace qp_solver {

std::vector<LinearQuadraticStage> getLinearQuadraticApproximation(CostFunctionBase& cost, SystemDynamicsBase& system,
                                                                  ConstraintBase* constraintsPtr,
                                                                  const ContinuousTrajectory& nominalTrajectory) {
  if (nominalTrajectory.timeTrajectory.empty()) {
    return {};
  }

  auto& t = nominalTrajectory.timeTrajectory;
  auto& x = nominalTrajectory.stateTrajectory;
  auto& u = nominalTrajectory.inputTrajectory;
  const int N = t.size() - 1;

  // LinearQuadraticProblem with N+1 elements. Terminal stage lqp[N].dynamics is ignored.
  std::vector<LinearQuadraticStage> lqp;
  lqp.reserve(N + 1);
  for (int k = 0; k < N; ++k) {  // Intermediate stages
    lqp.emplace_back(approximateStage(cost, system, constraintsPtr, {t[k], x[k], u[k]}, {t[k + 1], x[k + 1]}, k == 0));
  }

  if (constraintsPtr) {
    lqp.emplace_back(
        cost.finalCostQuadraticApproximation(t[N], x[N]), VectorFunctionLinearApproximation(),
        constraintsPtr->finalStateEqualityConstraintLinearApproximation(t[N], x[N]));  // Terminal cost and constraints, no dynamics.
  } else {
    lqp.emplace_back(cost.finalCostQuadraticApproximation(t[N], x[N]), VectorFunctionLinearApproximation(),
                     VectorFunctionLinearApproximation());  // Terminal cost, no dynamics and constraints.
  }

  return lqp;
}

LinearQuadraticStage approximateStage(CostFunctionBase& cost, SystemDynamicsBase& system, ConstraintBase* constraintsPtr,
                                      TrajectoryRef start, StateTrajectoryRef end, bool isInitialTime) {
  LinearQuadraticStage lqStage;
  auto dt = end.t - start.t;

  lqStage.cost = approximateCost(cost, start, dt);

  // Linearized Dynamics after discretization: x0[k+1] + dx[k+1] = A dx[k] + B du[k] + F(x0[k], u0[k])
  lqStage.dynamics = approximateDynamics(system, start, dt);
  // Adapt the offset to account for discretization and the nominal trajectory :
  // dx[k+1] = A dx[k] + B du[k] + F(x0[k], u0[k]) - x0[k+1]
  lqStage.dynamics.f -= end.x;

  if (constraintsPtr) {
    if (isInitialTime) {
      // only use stat-input constraints for initial time
      lqStage.constraints = constraintsPtr->stateInputEqualityConstraintLinearApproximation(start.t, start.x, start.u);
    } else {
      // concatenate stat-input and state-only constraints
      const auto stateInputConstraint = constraintsPtr->stateInputEqualityConstraintLinearApproximation(start.t, start.x, start.u);
      const auto stateConstraint = constraintsPtr->stateEqualityConstraintLinearApproximation(start.t, start.x);
      const auto numStateInputConstraints = stateInputConstraint.f.size();
      const auto numStateConstraints = stateConstraint.f.size();
      lqStage.constraints.resize(numStateConstraints + numStateInputConstraints, start.x.size(), start.u.size());
      lqStage.constraints.f.head(numStateInputConstraints) = stateInputConstraint.f;
      lqStage.constraints.dfdx.topRows(numStateInputConstraints) = stateInputConstraint.dfdx;
      lqStage.constraints.dfdu.topRows(numStateInputConstraints) = stateInputConstraint.dfdu;
      lqStage.constraints.f.tail(numStateConstraints) = stateConstraint.f;
      lqStage.constraints.dfdx.bottomRows(numStateConstraints) = stateConstraint.dfdx;
      lqStage.constraints.dfdu.bottomRows(numStateConstraints).setZero();
    }
  }

  return lqStage;
}

ScalarFunctionQuadraticApproximation approximateCost(CostFunctionBase& cost, TrajectoryRef start, scalar_t dt) {
  // Approximates the cost accumulation of the dt interval.
  // Use Euler integration
  const auto continuousCosts = cost.costQuadraticApproximation(start.t, start.x, start.u);
  ScalarFunctionQuadraticApproximation discreteCosts;
  discreteCosts.dfdxx = continuousCosts.dfdxx * dt;
  discreteCosts.dfdux = continuousCosts.dfdux * dt;
  discreteCosts.dfduu = continuousCosts.dfduu * dt;
  discreteCosts.dfdx = continuousCosts.dfdx * dt;
  discreteCosts.dfdu = continuousCosts.dfdu * dt;
  discreteCosts.f = continuousCosts.f * dt;
  return discreteCosts;
}

VectorFunctionLinearApproximation approximateDynamics(SystemDynamicsBase& system, TrajectoryRef start, scalar_t dt) {
  // Forward Euler discretization
  // x[k+1] = x[k] + dt * dxdt[k]
  // x[k+1] = (x0[k] + dx[k]) + dt * dxdt[k]
  // x[k+1] = (x0[k] + dx[k]) + dt * (A_c dx[k] + B_c du[k] + b_c)
  // x[k+1] = (I + A_c * dt) dx[k] + (B_c * dt) du[k] + (b_c * dt + x0[k])
  const auto continuousDynamics = system.linearApproximation(start.t, start.x, start.u);
  VectorFunctionLinearApproximation discreteDynamics;
  discreteDynamics.dfdx = continuousDynamics.dfdx * dt;
  discreteDynamics.dfdx.diagonal().array() += 1.0;
  discreteDynamics.dfdu = continuousDynamics.dfdu * dt;
  discreteDynamics.f = continuousDynamics.f * dt + start.x;
  return discreteDynamics;
}

}  // namespace qp_solver
}  // namespace ocs2
