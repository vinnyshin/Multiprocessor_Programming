//==================================================== file = genzipf.c =====
//=  Program to generate Zipf (power law) distributed random variables      =
//===========================================================================
//=  Author: Kenneth J. Christensen                                         =
//=          University of South Florida                                    =
//=          WWW: http://www.csee.usf.edu/~christen                         =
//=          Email: christen@csee.usf.edu                                   =
//=-------------------------------------------------------------------------=
//=  History: KJC (11/16/03) - Genesis (from genexp.c)                      =
//===========================================================================
//----- Include files -------------------------------------------------------
#include <assert.h>             // Needed for assert() macro
#include <stdio.h>              // Needed for printf()
#include <stdlib.h>             // Needed for exit() and ato*()
#include <math.h>               // Needed for pow()
#include <random>
#include <iostream>


//----- Constants -----------------------------------------------------------
#define  FALSE          0       // Boolean false
#define  TRUE           1       // Boolean true

double* pow_table;

namespace util {

class Zipf {
  public:
    Zipf(double alpha, int n): alpha_(alpha), n_(n), first_(true), c_(0) {
      gen_ = std::mt19937(rd_());
      dis_ = std::uniform_real_distribution<double>(0.0, 1.0);
      pow_table = new double[n];
      
      for (int i = 0; i < n; i++) {
        pow_table[i] = pow((double) (i + 1), alpha_);
      }
    }

    int Generate() {
      //===========================================================================
      //=  Function to generate Zipf (power law) distributed random variables     =
      //=    - Input: alpha and N                                                 =
      //=    - Output: Returns with Zipf distributed random variable              =
      //===========================================================================
      double z;                     // Uniform random number (0 < z < 1)
      double sum_prob;              // Sum of probabilities
      double zipf_value;            // Computed exponential value to be returned
      int    i;                     // Loop counter

      // Compute normalization constant on first call only
      if (first_ == true)
      {
        for (i=1; i<=n_; i++)
          c_ = c_ + (1.0 / pow_table[i-1]);
        c_ = 1.0 / c_;
        first_ = false;
      }

      // Pull a uniform random number (0 < z < 1)
      do
      {
        z = dis_(gen_);
      }
      while ((z == 0) || (z == 1));

      // Map z to the value
      sum_prob = 0;
      for (i=1; i<=n_; i++)
      {
        sum_prob = sum_prob + c_ / pow_table[i-1];
        if (sum_prob >= z)
        {
          zipf_value = i;
          break;
        }
      }

      // Assert that zipf_value is between 1 and N
      assert((zipf_value >=1) && (zipf_value <= n_));

      return(zipf_value);
    }

  private:
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_real_distribution<double> dis_;
    double alpha_;
    int n_;
    long x_;     // Random int value
    bool first_; // Static first time flag
    double c_;   // Normalization constant
};

}  // namespace util
