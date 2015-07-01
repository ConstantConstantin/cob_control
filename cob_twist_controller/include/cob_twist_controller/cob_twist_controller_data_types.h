/*!
 *****************************************************************
 * \file
 *
 * \note
 *   Copyright (c) 2015 \n
 *   Fraunhofer Institute for Manufacturing Engineering
 *   and Automation (IPA) \n\n
 *
 *****************************************************************
 *
 * \note
 *   Project name: care-o-bot
 * \note
 *   ROS stack name: cob_control
 * \note
 *   ROS package name: cob_twist_controller
 *
 * \author
 *   Author: Marco Bezzon, email: Marco.Bezzon@ipa.fraunhofer.de
 *
 * \date Date of creation: April, 2015
 *
 * \brief
 *   Different data types for CobTwistController to be used in other modules
 *
 ****************************************************************/

#ifndef COB_TWIST_CONTROLLER_DATA_TYPES_H_
#define COB_TWIST_CONTROLLER_DATA_TYPES_H_

#include <vector>
#include <stdint.h>
#include <Eigen/Core>
#include <Eigen/LU> // necessary to use several methods on EIGEN Matrices.
#include <kdl/chainjnttojacsolver.hpp>

#include "cob_twist_controller/task_stack/task_stack_controller.h"

#define MAX_CRIT true
#define MIN_CRIT false
#define MAIN_TASK_PRIO 200

const double DAMPING_LIMIT = 1.0e-12; ///< const. value for zero comparison with damping factor
const double ZERO_LIMIT = 1.0e-9;

typedef Eigen::Matrix<double,6,Eigen::Dynamic> t_Matrix6Xd;

typedef Eigen::Matrix<double,6,1> t_Vector6d;
typedef Eigen::Matrix<double,7,1> t_Vector7d;
typedef Eigen::Matrix<double,7,6> t_Matrix76d;

enum DampingMethodTypes {
    NONE = 0,
    CONSTANT = 1,
    MANIPULABILITY = 2,
    LSV = 3,
};

enum ContraintTypes {
    None = 0,
    WLN = 1,
    WLN_JLA = 2,
    GPM_JLA = 3,
    GPM_JLA_MID = 4,
    GPM_CA = 5,
    TASK_STACK_NO_GPM = 6,
    TASK_STACK_GPM = 7,
    TASK_2ND_PRIO = 8,
    DYN_TASKS_READJ = 9,
};

struct JointStates
{
    KDL::JntArray current_q_;
    KDL::JntArray last_q_;
    KDL::JntArray current_q_dot_;
    KDL::JntArray last_q_dot_;
};

struct InvDiffKinSolverParams {
    DampingMethodTypes damping_method;
    bool numerical_filtering;
    double damping_factor;
    double lambda_max;
    double w_threshold;
    double beta;
    double eps_damping;
    ContraintTypes constraint;
    double k_H;
    double eps_truncation;
    bool base_compensation;
    bool base_active;
    double base_ratio;

    double mu;

    // added limits from URDF file
    std::vector<double> limits_max;
    std::vector<double> limits_min;
    std::vector<double> limits_vel;
    std::vector<std::string> frame_names;


    // TODO: Check for better place
    TaskStackController_t *task_stack_controller;
};

struct ObstacleDistanceInfo
{
    double min_distance;
    // Eigen::Vector3d distance_vec;
    t_Vector6d distance_vec;
    t_Vector6d frame_twist;
    std::string frame_id;
    Eigen::Vector3d collision_pnt_vector;
};

struct TwistControllerParams {
    uint8_t dof;
    bool base_active;
    bool base_compensation;
    double max_vel_lin;
    double max_vel_rot;
    double max_vel_lin_base;
    double max_vel_rot_base;
    double tolerance;

    bool keep_direction;
    bool enforce_pos_limits;
    bool enforce_vel_limits;

    // added limits from URDF file
    std::vector<double> limits_vel;
    std::vector<double> limits_min;
    std::vector<double> limits_max;

    // added a vector to contain all frames of interest for collision checking.
    std::vector<std::string> collision_check_frames;
};

enum EN_ConstraintStates
{
    NORMAL,
    DANGER,
    CRITICAL,
};


class ConstraintState {
    public:

        ConstraintState()
        : current_(NORMAL), previous_(NORMAL), transition_(false)
        {

        }

        inline void setState(EN_ConstraintStates next_state)
        {
            this->transition_ = this->current_ != next_state;
            this->previous_ = this->current_;
            this->current_ = next_state;
        }

        inline EN_ConstraintStates getCurrent() const
        {
            return this->current_;
        }

        inline EN_ConstraintStates getPrevious() const
        {
            return this->previous_;
        }

        inline bool isTransition() const
        {
            return this->transition_;
        }

    private:
        EN_ConstraintStates current_;
        EN_ConstraintStates previous_;
        bool transition_;
};

#endif /* COB_TWIST_CONTROLLER_DATA_TYPES_H_ */
