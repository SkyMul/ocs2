#pragma once

#include <ocs2_mpc/MPC_BASE.h>
#include <ocs2_oc/pi_solver/PI_Settings.h>
#include <ocs2_oc/pi_solver/PiSolver.hpp>

namespace ocs2 {

template <size_t STATE_DIM, size_t INPUT_DIM>
class MPC_PI : public MPC_BASE<STATE_DIM, INPUT_DIM> {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  using Ptr = std::shared_ptr<MPC_PI<STATE_DIM, INPUT_DIM>>;

  using BASE = MPC_BASE<STATE_DIM, INPUT_DIM>;
  using DIMENSIONS = Dimensions<STATE_DIM, INPUT_DIM>;

  using scalar_t = typename DIMENSIONS::scalar_t;
  using scalar_array_t = typename DIMENSIONS::scalar_array_t;
  using state_vector_t = typename DIMENSIONS::state_vector_t;
  using state_vector_array2_t = typename DIMENSIONS::state_vector_array2_t;
  using input_vector_array2_t = typename DIMENSIONS::input_vector_array2_t;
  using controller_ptr_array_t = typename BASE::controller_ptr_array_t;

  using solver_base_t = Solver_BASE<STATE_DIM, INPUT_DIM>;
  using solver_t = PiSolver<STATE_DIM, INPUT_DIM>;
  using dynamics_t = typename solver_t::controlled_system_base_t;
  using cost_t = typename solver_t::cost_function_t;
  using constraint_t = typename solver_t::constraint_t;

  MPC_PI(typename dynamics_t::Ptr dynamics, std::unique_ptr<cost_t> cost, const constraint_t constraint,
         const scalar_array_t& partitioningTimes, const MPC_Settings& mpcSettings, PI_Settings piSettings)
      : BASE(partitioningTimes, mpcSettings) {
    piSolverPtr_.reset(new solver_t(dynamics, std::move(cost), constraint, std::move(piSettings)));
    BASE::setBaseSolverPtr(piSolverPtr_.get());
  }

  virtual ~MPC_PI() = default;

  /**
   * Solves the optimal control problem for the given state and time period ([initTime,finalTime]).
   *
   * @param [out] initTime: Initial time. This value can be adjusted by the optimizer.
   * @param [in] initState: Initial state.
   * @param [out] finalTime: Final time. This value can be adjusted by the optimizer.
   * @param [out] timeTrajectoriesStockPtr: A pointer to the optimized time trajectories.
   * @param [out] stateTrajectoriesStockPtr: A pointer to the optimized state trajectories.
   * @param [out] inputTrajectoriesStockPtr: A pointer to the optimized input trajectories.
   * @param [out] controllerStockPtr: A pointer to the optimized control policy.
   */
  virtual void calculateController(const scalar_t& initTime, const state_vector_t& initState, const scalar_t& finalTime,
                                   const std::vector<scalar_array_t>*& timeTrajectoriesStockPtr,
                                   const state_vector_array2_t*& stateTrajectoriesStockPtr,
                                   const input_vector_array2_t*& inputTrajectoriesStockPtr,
                                   const controller_ptr_array_t*& controllerStockPtr) override {
    scalar_array_t partitioningTimesDummy;
    piSolverPtr_->run(initTime, initState, finalTime, partitioningTimesDummy);

    piSolverPtr_->getNominalTrajectoriesPtr(timeTrajectoriesStockPtr, stateTrajectoriesStockPtr, inputTrajectoriesStockPtr);
    piSolverPtr_->getControllerPtr(controllerStockPtr);
  }

  /**
   * Gets a pointer to the underlying solver used in the MPC.
   *
   * @return A pointer to the underlying solver used in the MPC
   */
  virtual solver_base_t* getSolverPtr() override { return piSolverPtr_.get(); }  // TODO(jcarius) should this happen in the base class?

 protected:
  std::unique_ptr<solver_t> piSolverPtr_;
};

}  // namespace ocs2
