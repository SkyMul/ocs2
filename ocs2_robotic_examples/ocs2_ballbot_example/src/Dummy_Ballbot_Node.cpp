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

#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mrt/MRT_ROS_Interface.h>
#include <ocs2_comm_interfaces/test/MRT_ROS_Dummy_Loop.h>

#include "ocs2_ballbot_example/BallbotInterface.h"
#include "ocs2_ballbot_example/definitions.h"
#include "ocs2_ballbot_example/ros_comm/BallbotDummyVisualization.h"

int main(int argc, char** argv) {
  const std::string robotName = "ballbot";
  using interface_t = ocs2::ballbot::BallbotInterface;
  using vis_t = ocs2::ballbot::BallbotDummyVisualization;
  using mrt_t = ocs2::MRT_ROS_Interface<ocs2::ballbot::STATE_DIM_, ocs2::ballbot::INPUT_DIM_>;
  using dummy_t = ocs2::MRT_ROS_Dummy_Loop<ocs2::ballbot::STATE_DIM_, ocs2::ballbot::INPUT_DIM_>;

  // task file
  if (argc <= 1) {
    throw std::runtime_error("No task file specified. Aborting.");
  }
  std::string taskFileFolderName = std::string(argv[1]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  // Initialize ros node
  ros::init(argc, argv, robotName + "_mrt");
  ros::NodeHandle n;

  // ballbotInterface
  interface_t ballbotInterface(taskFileFolderName);

  // MRT
  mrt_t mrt(robotName);
  mrt.initRollout(&ballbotInterface.getRollout());
  mrt.launchNodes(n);

  // Visualization
  std::shared_ptr<vis_t> ballbotDummyVisualization(new vis_t(n));

  // Dummy ballbot
  dummy_t dummyBallbot(mrt, ballbotInterface.mpcSettings().mrtDesiredFrequency_, ballbotInterface.mpcSettings().mpcDesiredFrequency_);
  dummyBallbot.subscribeObservers({ballbotDummyVisualization});

  // initial state
  mrt_t::system_observation_t initObservation;
  initObservation.state() = ballbotInterface.getInitialState();

  // initial command
  ocs2::CostDesiredTrajectories initCostDesiredTrajectories({initObservation.time()}, {initObservation.state()}, {initObservation.input()});

  // Run dummy (loops while ros is ok)
  dummyBallbot.run(initObservation, initCostDesiredTrajectories);

  // Successful exit
  return 0;
}
