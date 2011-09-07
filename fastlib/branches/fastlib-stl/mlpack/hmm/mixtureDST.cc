/**
 * @file mixtureDST.cc
 *
 * This file contains implementation of functions in mixtureDST.h
 */

#include <fastlib/fastlib.h>
#include <armadillo>
#include "fastlib/fx/io.h"
#include "support.h"
#include "mixtureDST.h"

using namespace hmm_support;

void MixtureGauss::Init(index_t K, index_t N) {
  for (index_t i = 0; i < K; i++) {
    arma::vec v;
    RAND_NORMAL_01_INIT(N, v);
    means.push_back(v);
  }
  
  for (index_t i = 0; i < means.size(); i++) {
    arma::mat m(N, N);
    m.zeros();
    for (index_t j = 0; j < N; j++)
      m(j, j) = 1.0;

    covs.push_back(m);
  }

  prior.set_size(means.size());
  prior.fill(1.0 / K);

  ACC_means = means;
  ACC_covs = covs;
  ACC_prior.set_size(K);
  inv_covs = covs;
  det_covs.set_size(covs.size());

  for (index_t i = 0; i < K; i++) {
    inv_covs[i] = inv(covs[i]);
    det_covs[i] = pow(2.0*math::PI, -N/2.0) * pow(det(covs[i]), -0.5);
  }
}

void MixtureGauss::Init(index_t K, const arma::mat& data, const std::vector<index_t>& labels) {
  index_t N = data.n_rows;
  for (index_t i = 0; i < K; i++) {
    arma::vec v(N);
    means.push_back(v);
  }
  
  for (index_t i = 0; i < means.size(); i++) {
    arma::mat m(N, N);
    covs.push_back(m);
  }

  prior.set_size(means.size());

  ACC_means = means;
  ACC_covs = covs;
  ACC_prior.set_size(K);
  inv_covs = covs;
  det_covs.set_size(covs.size());

  start_accumulate();
  //printf("cols = %d rows = %d\n", data.n_cols(), data.n_rows()); 
  for (index_t i = 0; i < data.n_cols; i++) {
    arma::vec v = data.unsafe_col(i);
    //printf("%d\n", i);
    accumulate_cluster(labels[i], v);
  }
  end_accumulate_cluster();
}

void MixtureGauss::InitFromFile(const char* mean_fn, const char* covs_fn, const char* prior_fn) {
  arma::mat meansmat;
  data::Load(mean_fn, meansmat);

  mat2arrlst(meansmat, means);
  index_t N = means[0].n_elem;
  index_t K = means.size();
  if (covs_fn != NULL) {
    arma::mat covsmat;
    data::Load(covs_fn, covsmat);

    mat2arrlstmat(N, covsmat, covs);
    mlpack::IO::AssertMessage(K == covs.size(), "MixtureGauss::InitFromFile(): sizes do not match!");
  } else {
    for (index_t i = 0; i < means.size(); i++) {
      arma::mat m(N, N);
      m.zeros();
      
      for (index_t j = 0; j < N; j++)
        m(j, j) = 1.0;

      covs.push_back(m);
    }
  }

  if (prior_fn != NULL) {
    arma::mat priormat;
    data::Load(prior_fn, priormat);
    
    mlpack::IO::AssertMessage(K == priormat.n_cols, "MixtureGauss::InitFromFile(): sizes do not match!");

    prior.set_size(K);

    for (index_t i = 0; i < K; i++)
      prior[i] = priormat(0, i);
  } else {
    prior.set_size(means.size());
    prior.fill(1.0 / K);
  }
  
  ACC_means = means;
  ACC_covs = covs;
  ACC_prior.set_size(K);
  inv_covs = covs;
  det_covs.set_size(covs.size());

  for (index_t i = 0; i < K; i++) {
    inv_covs[i] = inv(covs[i]);
    det_covs[i] = pow(2.0 * math::PI, -N / 2.0) * pow(det(covs[i]), -0.5);
  }
}

void MixtureGauss::InitFromProfile(const std::vector<arma::mat>& matlst, index_t start, index_t N) {
  mlpack::IO::Assert(matlst[start].n_cols == 1);
  arma::vec tmp = matlst[start].unsafe_col(0);
  prior = tmp;

  // DEBUG: print_vector(prior, "  prior = ");

  index_t K = prior.n_elem;
  for (index_t i = start + 1; i < start + (2 * K + 1); i += 2) {
    mlpack::IO::Assert(matlst[i].n_rows == N && matlst[i].n_cols == 1);
    mlpack::IO::Assert(matlst[i + 1].n_rows == N && matlst[i + 1].n_cols == N);

    arma::vec m = matlst[i].unsafe_col(0);
    means.push_back(m);
    covs.push_back(matlst[i + 1]);    
  }

  ACC_means = means;
  ACC_covs = covs;
  ACC_prior.set_size(K);
  inv_covs = covs;
  det_covs.set_size(covs.size());

  for (index_t i = 0; i < K; i++) {
    inv_covs[i] = inv(covs[i]);
    det_covs[i] = pow(2.0 * math::PI, -N / 2.0) * pow(det(covs[i]), -0.5);
  }  
}

void MixtureGauss::print_mixture(const char* s) const {
  index_t K = means.size();
  printf("%s - Mixture (%"LI")\n", s, K);
  print_vector(prior, "  PRIOR");
  for (index_t i = 0; i < K; i++) {
    printf("  CLUSTER %"LI":\n", i);
    print_vector(means[i], "  MEANS");
    print_matrix(covs[i], "  COVS");
  }
}

void MixtureGauss::generate(arma::vec& v) const {
  index_t K = means.size();
  double r = RAND_UNIFORM_01();
  index_t cluster = K - 1;
  double s = 0;
  for (index_t i = 0; i < K; i++) {
    s += prior[i];
    if (s >= r) {
      cluster = i;
      break;
    }
  }
  RAND_NORMAL_INIT(means[cluster], covs[cluster], v);
}

double MixtureGauss::getPDF(const arma::vec& v) const {
  index_t K = means.size();
  double s = 0;
  for (index_t i = 0; i < K; i++)
    s += getPDF(i, v);

  return s;
}

double MixtureGauss::getPDF(index_t cluster, const arma::vec& v) const {
  return prior[cluster] * NORMAL_DENSITY(v, means[cluster], inv_covs[cluster], det_covs[cluster]);
}
