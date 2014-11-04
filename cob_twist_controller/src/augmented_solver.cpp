#include "ros/ros.h"

#include "cob_twist_controller/augmented_solver.h"
#include <fstream>

#define DEBUG true


augmented_solver::augmented_solver(const KDL::Chain& _chain, double _eps, int _maxiter):
    chain(_chain),
    jac(chain.getNrOfJoints()),
    jnt2jac(chain),
    eps(_eps),
    maxiter(_maxiter),
    initial_iteration(true),
    base_active_(false),
    base_ratio_(0.0)
{}

augmented_solver::~augmented_solver()
{}


int augmented_solver::CartToJnt(const KDL::JntArray& q_in, KDL::Twist& v_in, KDL::JntArray& qdot_out)
{
    ///Let the ChainJntToJacSolver calculate the jacobian "jac_chain" for the current joint positions "q_in"
    KDL::Jacobian jac_chain(chain.getNrOfJoints());
    jnt2jac.JntToJac(q_in, jac_chain);
    
    if(base_active_)
    {
        //Create standard platform jacobian
        Eigen::Matrix<double,6,3> jac_base;
        jac_base.setZero();

        jac_base(0,0) = base_ratio_;    //linear_x
        jac_base(1,1) = base_ratio_;    //linear_y
        jac_base(5,2) = base_ratio_;    //angular_z

        //combine chain Jacobian and platform Jacobian
        Eigen::Matrix<double, 6, Eigen::Dynamic> jac_full;
        jac_full.resize(6,chain.getNrOfJoints() + jac_base.cols());
        jac_full << jac_chain.data, jac_base;
        
        //std::cout << "Combined jacobian:\n " << jac_full << "\n";
        //ROS_INFO_STREAM("JacBase: rows " <<jac_base.rows()<<"; cols "<<jac_base.cols());
        //ROS_INFO_STREAM("JacFull: rows " <<jac_full.rows()<<"; cols "<<jac_full.cols());
        
        //ROS_INFO_STREAM("JacANTE: rows " <<jac.rows()<<"; cols "<<jac.columns());
        jac.resize(chain.getNrOfJoints() + jac_base.cols());
        //ROS_INFO_STREAM("JacPOST: rows " <<jac.rows()<<"; cols "<<jac.columns());
        
        jac.data << jac_full;
    }
    else
    {
        jac.resize(chain.getNrOfJoints());
        jac.data << jac_chain.data;
    }
    
    ///Use Eigen::JacobiSVD
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(jac.data, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::VectorXd S = svd.singularValues();
    Eigen::VectorXd v_in_vec = Eigen::VectorXd::Zero(jac.rows());
    Eigen::MatrixXd S_inv = Eigen::MatrixXd::Zero(jac.columns(),jac.rows());
    
    ///truncated S(i)
    for (int i=0; i<(jac.rows()<jac.columns()?jac.rows():jac.columns());i++)
    {    S_inv(i,i)=((S(i)<eps)?0:1/S(i));    }
    
    ///convert input
    for (int i=0; i<jac.rows(); i++)
    {    v_in_vec(i)=v_in(i);    }
    
    Eigen::MatrixXd qdot_out_vec= svd.matrixV()*S_inv*svd.matrixU().transpose()*v_in_vec;
    
    ///convert output
    for(int i=0; i<jac.columns(); i++)
    {    qdot_out(i)=qdot_out_vec(i,0);    }
    
    //if(DEBUG) 
    //{
        /////compute manipulability
        /////kappa = sqrt(norm(J*Jt)) 
        /////see  T.Yoshikawa "Manipulability of robotic mechanisms"
        /////     International Journal of Robotics Research, 4(2):3-9, 1985
        //Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> prod = jac.data * jac.data.transpose();
        //double d = prod.determinant();
        //double kappa = std::sqrt(d);
        //std::cout << "\nManipulability: " << kappa << "\n";
        
        //std::cout << "Singular Values:\n " << S << "\n";
        //std::cout << "Singular Values inv:\n " << S_inv << "\n";
        //std::cout << "Result:\n " << qdot_out_vec << "\n";
    //}
    
    return 1;
}


int augmented_solver::CartToJnt(const KDL::JntArray& q_in, KDL::Twist& v_in, KDL::JntArray& qdot_out, std::string damping_method = "trackingError")
{
    ///used only for debugging
    std::ofstream file("test_end_effect.txt", std::ofstream::app);
    
    ///Let the ChainJntToJacSolver calculate the jacobian "jac" for the current joint positions "q_in"
    jnt2jac.JntToJac(q_in,jac);

    Eigen::MatrixXd qdot_out_vec;
    int task_size = 1;
            
    Eigen::MatrixXd We = Eigen::MatrixXd::Identity(jac.rows(), jac.rows());
    Eigen::MatrixXd Wc = Eigen::MatrixXd::Zero(task_size,task_size);
    Eigen::MatrixXd Wv = Eigen::MatrixXd::Identity(jac.columns(), jac.columns());
    Eigen::MatrixXd Jc = Eigen::MatrixXd::Zero(task_size,jac.columns());

    Eigen::JacobiSVD<Eigen::MatrixXd> svd(jac.data,Eigen::ComputeFullU | Eigen::ComputeFullV);
    
    Eigen::VectorXd v_in_vec = Eigen::VectorXd::Zero(jac.rows());
    Eigen::VectorXd S = svd.singularValues();
    
    double damping_factor = 0.0;
    if (damping_method=="manipulability")
    {
        Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> prod = jac.data * jac.data.transpose();
        double d = prod.determinant();        
        double w = std::sqrt(std::abs(d));      
        double lambda0 = 0.11;
        double wt = 0.0005;
        damping_factor = (w<wt ? lambda0 * pow((1 - w/wt),2) : 0);
        //std::cout << "w" << w << " wt" <<wt << " Condicion" << (bool)(w<wt) << "\n";
    }
    
    else if (damping_method=="manipulabilityRate")
    {
        Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> prod = jac.data * jac.data.transpose();
        double d = prod.determinant();
        double w = std::sqrt(std::abs(d));
        double wt = 1.2;
        double lambda0 = 0.008;
        if (initial_iteration)
        {
            damping_factor=lambda0;
            initial_iteration = false;
        }
        else
        {
            if(wkm1==0) //division by zero
            {    damping_factor = lambda0;    }
            else
            {    damping_factor = (std::abs(w/wkm1) > wt ? lambda0 * (1-w/wkm1) : 0);    }
        }
        
        std::cout<<"w: "<<w<<" wk-1: "<< wkm1 << " condition:" << (bool)(w/wkm1 <wt) << std::endl;
        wkm1=w;
    }
    
    else if (damping_method == "trackingError")    
    {
        double deltaRMax = 0.05;
        damping_factor = (svd.singularValues())(0);
        for (int i=1; i<jac.rows();i++) 
        {                    
            if(damping_factor>(svd.singularValues())(i) && ((svd.singularValues())(i))>eps) //What is a zero singular value, and what is not. Less than 0.005 seems OK
            {
                damping_factor = (svd.singularValues())(i);            
                //file << "Minimum sv:"<< damping_factor << std::endl;
            }
        }        
                
        damping_factor=pow(damping_factor,2)*deltaRMax/(1-deltaRMax);    
    }
    
    else if (damping_method == "singularRegion")
    {
        //Eigen::VectorXd S = svd.singularValues();
        double lambda0 = 0.01;        
        damping_factor = S(S.size()-1)>=eps ? 0 : sqrt(1-pow(S(S.size()-1)/eps,2))*lambda0; //The last singular value seems to be less than 0.01 near singular configurations
    }
    
    else if (damping_method == "constant")
    {
        damping_factor = 0.01;
    }
    
    ///use calculated damping value lambda for SVD
    Wv=Wv*damping_factor*damping_factor;
    Eigen::MatrixXd tmp = (jac.data.transpose()*We*jac.data+Jc.transpose()*Wc*Jc+Wv).inverse();
    
    ///catch isNaN
    //if(isnan(tmp.determinant()))
    //{    return -1;    }
    
    ///convert input
    for (int i=0; i<jac.rows(); i++)
    {    v_in_vec(i)=v_in(i);    }
    
    //if(damping_factor==0) 
    //{
        //Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> prod = jac.data.transpose() * jac.data;
        //Eigen::JacobiSVD<Eigen::MatrixXd> prod_svd(prod,Eigen::ComputeFullU | Eigen::ComputeFullV);
        //Eigen::VectorXd prod_S = prod_svd.singularValues();
        //Eigen::MatrixXd prod_S_inv = Eigen::MatrixXd::Zero(jac.columns(),jac.columns());
        //for (int i=0; i<jac.columns()-1; i++)
        //{   prod_S_inv(i,i)=(prod_S(i)==0?0:1/prod_S(i));   }
        //
        //qdot_out_vec= prod_svd.matrixV()*prod_S_inv*prod_svd.matrixU().transpose()*(jac.data.transpose()*We*v_in_vec);
        
        Eigen::MatrixXd sum = Eigen::MatrixXd::Zero(jac.columns(),jac.rows());
        
        for (int i=0; i<jac.rows() ; i++)
        {
            for (int j=0; j<jac.rows() ;j++)
                for (int k=0; k<jac.columns(); k++)
                    sum(k,j)+=S(i)*svd.matrixV()(k,i)*svd.matrixU()(j,i)*(pow(S(i),2)+pow(damping_factor,2)==0?0:1/(pow(S(i),2)+pow(damping_factor,2)));
        }
        
        qdot_out_vec= sum*v_in_vec;
    //}
    //else
    //{
        //qdot_out_vec= tmp*(jac.data.transpose()*We*v_in_vec);
    //}
    
    
    ///convert output
    for(int i=0; i<jac.columns(); i++)
    {    qdot_out(i)=qdot_out_vec(i,0);    }
    
    
    ///write debug info to file
    for(int i=0; i<jac.columns(); i++)
    {    
        if(std::abs(qdot_out_vec(i,0))>500)
        {
            if (file.is_open())
            {
                Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> prod = jac.data * jac.data.transpose();                    
                
                double d = prod.determinant();
                double kappa = std::sqrt(d);
                file << "Here is the Jacobian:\n" << jac.data << '\n';
                file << "Last Jacobian:\n" << Jcm1 << '\n';
                file << "Here is prod:\n" << prod << '\n';
                file << "And here is the other stuff:"<< std::endl << "damping_factor" <<damping_factor<<std::endl;
                file << "Singular values:\n" << svd.singularValues();
                file << "\nManipulability: " << kappa << "\n";
                file<<"El twist deberia ser:\n"<<jac.data*qdot_out_vec<<std::endl;
                file << "La entrada fue: \n" << v_in_vec<<std::endl;
                file << "La salida es: \n" << qdot_out_vec <<'\n';
                file << "inv(jac' jac):\n" << (jac.data.transpose()*jac.data).inverse();
            }
            break;
        }
    }
    
    if(DEBUG)
    {
        //compute manipulability
        ///kappa = sqrt(norm(J*Jt))
        ///see  T.Yoshikawa "Manipulability of robotic mechanisms"
        ///     International Journal of Robotics Research, 4(2):3-9, 1985
        //Eigen::Matrix<double,Eigen::Dynamic,Eigen::Dynamic> prod = jac.data * jac.data.transpose();
        //double d = prod.determinant();
        //double kappa = std::sqrt(d);
        //std::cout << "\nManipulability: " << kappa << "\n";
        //ROS_WARN("Damping factor: %f",damping_factor);
        //std::cout<<"task_size:"<<task_size<<std::endl;
        //std::cout << "Current jacobian:\n " << jac.data << "\n";
        //std::cout << "Current We:\n " << We << "\n";
        //std::cout << "Current Wc:\n " << Wc << "\n";
        //std::cout << "Current Wv:\n " << Wv << "\n";
        //std::cout << "Jc:\n " << Jc << "\n";
        //std::cout<<"Singular values"<<svd.singularValues()<<std::endl;
        std::cout << "Damping factor" << damping_factor << std::endl;
        //std::cout << "Reciprocal Condition number" << 1/(prod.norm()*prod.inverse().norm())<<'\n';
        //std::cout << "DEBUG END" << std::endl;
    }
    
    return 1;
}




