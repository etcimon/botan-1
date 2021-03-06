/*
* DSA Parameter Generation
* (C) 1999-2007 Jack Lloyd
*
* Distributed under the terms of the Botan license
*/

#include <botan/numthry.h>
#include <botan/algo_factory.h>
#include <botan/hash.h>
#include <botan/parsing.h>
#include <algorithm>

namespace Botan {

namespace {

/*
* Check if this size is allowed by FIPS 186-3
*/
bool fips186_3_valid_size(size_t pbits, size_t qbits)
   {
   if(qbits == 160)
      return (pbits == 512 || pbits == 768 || pbits == 1024);

   if(qbits == 224)
      return (pbits == 2048);

   if(qbits == 256)
      return (pbits == 2048 || pbits == 3072);

   return false;
   }

}

/*
* Attempt DSA prime generation with given seed
*/
bool generate_dsa_primes(RandomNumberGenerator& rng,
                         Algorithm_Factory& af,
                         BigInt& p, BigInt& q,
                         size_t pbits, size_t qbits,
                         const std::vector<byte>& seed_c)
   {
   if(!fips186_3_valid_size(pbits, qbits))
      throw Invalid_Argument(
         "FIPS 186-3 does not allow DSA domain parameters of " +
         std::to_string(pbits) + "/" + std::to_string(qbits) + " bits long");

   if(seed_c.size() * 8 < qbits)
      throw Invalid_Argument(
         "Generating a DSA parameter set with a " + std::to_string(qbits) +
         "long q requires a seed at least as many bits long");

   std::unique_ptr<HashFunction> hash(
      af.make_hash_function("SHA-" + std::to_string(qbits)));

   const size_t HASH_SIZE = hash->output_length();

   class Seed
      {
      public:
         Seed(const std::vector<byte>& s) : seed(s) {}

         operator std::vector<byte>& () { return seed; }

         Seed& operator++()
            {
            for(size_t j = seed.size(); j > 0; --j)
               if(++seed[j-1])
                  break;
            return (*this);
            }
      private:
         std::vector<byte> seed;
      };

   Seed seed(seed_c);

   q.binary_decode(hash->process(seed));
   q.set_bit(qbits-1);
   q.set_bit(0);

   if(!is_prime(q, rng))
      return false;

   const size_t n = (pbits-1) / (HASH_SIZE * 8),
                b = (pbits-1) % (HASH_SIZE * 8);

   BigInt X;
   std::vector<byte> V(HASH_SIZE * (n+1));

   for(size_t j = 0; j != 4096; ++j)
      {
      for(size_t k = 0; k <= n; ++k)
         {
         ++seed;
         hash->update(seed);
         hash->final(&V[HASH_SIZE * (n-k)]);
         }

      X.binary_decode(&V[HASH_SIZE - 1 - b/8],
                      V.size() - (HASH_SIZE - 1 - b/8));
      X.set_bit(pbits-1);

      p = X - (X % (2*q) - 1);

      if(p.bits() == pbits && is_prime(p, rng))
         return true;
      }
   return false;
   }

/*
* Generate DSA Primes
*/
std::vector<byte> generate_dsa_primes(RandomNumberGenerator& rng,
                                      Algorithm_Factory& af,
                                      BigInt& p, BigInt& q,
                                      size_t pbits, size_t qbits)
   {
   while(true)
      {
      std::vector<byte> seed(qbits / 8);
      rng.randomize(&seed[0], seed.size());

      if(generate_dsa_primes(rng, af, p, q, pbits, qbits, seed))
         return seed;
      }
   }

}
