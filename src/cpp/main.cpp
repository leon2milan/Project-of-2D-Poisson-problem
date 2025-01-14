#include <iostream>
#include <vector>
#include <Eigen/Dense>
#include<Eigen/IterativeLinearSolvers>
#include <Eigen/SparseCholesky>
#include<Eigen/SparseQR>
#include <Eigen/OrderingMethods>



#include <chrono>
#include <time.h>

#include "parameters.h"
#include "Set_2DAV_diags.h"

using Eigen::VectorXd;
using Eigen::MatrixXd;
using Eigen::SparseMatrix;
using Eigen::BiCGSTAB;
using Eigen::MatrixXd;
using Eigen::SimplicialCholeskyLDLT;


typedef Eigen::Triplet<double> Trp;  //TrpripleTrps are objecTrp s of form: row index, column index, value.  used to fill sparse matrices

int main()
{

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();  //start clock timer

    VectorXd x(num_elements), b(num_elements);
    SparseMatrix<double> A(num_elements,num_elements);
    //The creation of these 2 matrices is super slow!So
    MatrixXd epsilon = MatrixXd::Ones(num_cell+1,num_cell+1);  //NOTE: this is num_cell+1*num_cell+1 (NOT num_elements^2), 1 element per each physical grid pt. make matrix of ones
    MatrixXd netcharge = MatrixXd::Zero(num_cell+1,num_cell+1);  //make matrix of zeroes

    std::vector<double> main_diag (num_elements+1);
    std::vector<double> upper_diag(num_elements);
    std::vector<double> lower_diag(num_elements);
    std::vector<double> far_lower_diag(num_elements-N+1);
    std::vector<double> far_upper_diag(num_elements-N+1);

    std::vector<double> V(num_elements+1); //vector for solution, electric potential
    std::vector<double> rhs(num_elements+1);

    //fill A and b
    //Define boundary conditions and initial conditions
    double Va = 1.;
    double V_bottomBC = 0.;
    double V_topBC= Va/Vt;

    //Initial conditions
    double diff = (V_topBC - V_bottomBC)/num_cell;
    int index = 0;
    for (int j = 1;j<=N;j++){//  %corresponds to z coord
        index++;
        V[index] = diff*j;
        for (int i = 2;i<=N;i++){//  %elements along the x direction assumed to have same V
            index++;
            V[index] = V[index-1];
        }
    }

    //side BCs, insulating BC's
    std::vector<double> V_leftBC(N+1);
    std::vector<double> V_rightBC(N+1);

    for(int i = 1;i<=N;i++){
        V_leftBC[i] = V[(i-1)*N +  1];
        V_rightBC[i] = V[i*N];
    }


    //Set up matrix equation
    main_diag = set_main_Vdiag(epsilon, main_diag);
    upper_diag = set_upper_Vdiag(epsilon, upper_diag);
    lower_diag = set_lower_Vdiag(epsilon, lower_diag);
    far_upper_diag = set_far_upper_Vdiag(epsilon, far_upper_diag);
    far_lower_diag = set_far_lower_Vdiag(epsilon, far_lower_diag);


    //setup rhs of Poisson eqn.
    int index2 = 0;
    for(int j = 1;j<=N;j++){
        if(j==1){
            for(int i = 1;i<=N;i++){
                index2++;                //THIS COULD BE MODIFIES TO a switch ,case statement--> might be cleaner
                if(i==1){
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*(V_leftBC[1] + V_bottomBC);
                }else if(i == N)
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*(V_rightBC[1] + V_bottomBC);
                else
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*V_bottomBC;
            }
        }else if(j==N){
            for(int i = 1; i<=N;i++){
                index2++;
                if(i==1)
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*(V_leftBC[N] + V_topBC);
                else if(i == N)
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*(V_rightBC[N] + V_topBC);
                else
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*V_topBC;
            }
        }else{
            for(int i = 1;i<=N;i++){
                index2++;
                if(i==1)
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*V_leftBC[j];
                else if(i == N)
                    rhs[index2] = netcharge(i,j) + epsilon(i,j)*V_rightBC[j];
                else
                    rhs[index2] = netcharge(i,j);
            }
        }
    }

   //setup the triplet list for sparse matrix
   
    std::vector<Trp> triplet_list(5*num_elements);   //approximate the size that need         // list of non-zeros coefficients in triplet form(row index, column index, value)
    int trp_cnt = 0;
    for(int i = 1; i< main_diag.size(); i++){
        b(i-1) = rhs[i];   //fill VectorXd  rhs of the equation
          triplet_list[trp_cnt] = {i-1, i-1, main_diag[i]};
          trp_cnt++;
        //triplet_list.push_back(Trp(i-1,i-1,main_diag[i]));;   //triplets for main diag
    }
    for(int i = 1;i< upper_diag.size();i++){
        triplet_list[trp_cnt] = {i-1, i, upper_diag[i]};
        trp_cnt++;
        //triplet_list.push_back(Trp(i-1, i, upper_diag[i]));  //triplets for upper diagonal
     }
     for(int i = 1;i< lower_diag.size();i++){
         triplet_list[trp_cnt] = {i, i-1, lower_diag[i]};
         trp_cnt++;
        // triplet_list.push_back(Trp(i, i-1, lower_diag[i]));
     }
     for(int i = 1;i< far_upper_diag.size();i++){
         triplet_list[trp_cnt] = {i-1, i-1+N, far_upper_diag[i]};
         trp_cnt++;
         //triplet_list.push_back(Trp(i-1, i-1+N, far_upper_diag[i]));
         triplet_list[trp_cnt] = {i-1+N, i-1, far_lower_diag[i]};
         trp_cnt++;
         //triplet_list.push_back(Trp(i-1+N, i-1, far_lower_diag[i]));
      }


    A.setFromTriplets(triplet_list.begin(), triplet_list.end());    //A is our sparse matrix

    //using conjugate gradient--> this is faster than BiCGSTAB
/*
    Eigen::ConjugateGradient<SparseMatrix<double>, Eigen::UpLoType::Lower|Eigen::UpLoType::Upper > cg;
    cg.compute(A);
   x = cg.solve(b);
    std::cout << "#iterations:     " << cg.iterations() << std::endl;
     std::cout << "estimated error: " << cg.error()      << std::endl;

*/

  /*  //using sparse LU
     Eigen::SparseLU<SparseMatrix<double>>  sLU;
    sLU.analyzePattern(A);
    sLU.factorize(A);
    x = sLU.solve(b);
    */

   //using Sparse Cholesky  (THIS SEEMS TO BE THE FASTEST for the 2D Poisson problem)
    Eigen::SimplicialLDLT<SparseMatrix<double>, Eigen::UpLoType::Lower, Eigen::AMDOrdering<int>> SCholesky; //Note using NaturalOrdering is much much slower
    SCholesky.analyzePattern(A);
    SCholesky.factorize(A);
    x = SCholesky.solve(b);


    //NOTE, for repeat solves, I might not need to keep factorizing! Can just subtitute in the new rhs every time, and use solve()



    /*
    //using SparseQR  (is very slow, compared to others for large matrices).
    Eigen::SparseQR<SparseMatrix<double>, Eigen::COLAMDOrdering<int>> SQR;  //NOTE: this ordering parameter is needed for this to work
    SQR.analyzePattern(A);
    SQR.factorize(A);
    x = SQR.solve(b);
    */

    //using BiCGSTAB: this is probably useful for non-symmetric matrices, i.e. not Poisson equation which is symmetric. In that case, Cholesky might not work.

    BiCGSTAB<SparseMatrix<double>, Eigen::IncompleteLUT<double>> solver;  //BiCGStab solver object
    solver.compute(A);  //this computes the preconditioner
    x = solver.solve(b);
    std::cout << "#iterations:     " << solver.iterations() << std::endl;
    std::cout << "estimated error: " << solver.error()      << std::endl;


    std::cout << x <<std::endl;  //they have an overloaded << for seeing result!


    std::chrono::high_resolution_clock::time_point finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = std::chrono::duration_cast<std::chrono::duration<double>>(finish-start);
    std::cout << "1 Va CPU time = " << time.count() << std::endl;


    return 0;
}