/**
 * @file __uint128_t_support.hh
 * @brief Provides std::ostream operator<< and
 * std::hash specialization for __uint128_t.
 *
 * This file adds necessary support for
 * the GCC/Clang extension type __uint128_t
 * to work seamlessly with standard library facilities
 * like std::cout and std::unordered_map.
 * It should be included in projects that
 *  utilize __uint128_t and require these features.
 */

 #ifndef __BASE_INT128_SUPPORT_HH__
 #define __BASE_INT128_SUPPORT_HH__

 // Check if the compiler supports __int128
 // This is typically true for GCC and Clang on 64-bit architectures
 #ifdef __SIZEOF_INT128__

 #include <iostream>
 #include <iomanip>
 #include <sstream>
 #include <cstdint>
 #include <functional> // For std::hash

 // --- Provide std::ostream& operator<< for __uint128_t ---
 namespace std {

 /**
  * @brief Overloads the << operator to print __uint128_t values.
  *
  * Prints the value in hexadecimal format (0x...).
  * If the high 64 bits are zero,
  * only the relevant low bits are
  * shown without leading zeros (except for value 0).
  */
 inline std::ostream&
 operator<<(std::ostream& os, const __uint128_t& value)
 {
     // Decompose into high and low 64-bit parts
     const uint64_t low  = static_cast<uint64_t>(value);
     const uint64_t high = static_cast<uint64_t>(value >> 64);

     // Save the current stream flags to restore them later
     const std::ios_base::fmtflags old_flags = os.flags();

     os << "0x" << std::hex << std::setfill('0');

     if (high != 0) {
         // If high part is non-zero, print both parts with fixed width
         os << std::setw(16) << high << std::setw(16) << low;
     } else {
         // If high part is zero, print only the low part
         // omitting leading zeros
         if (low == 0) {
             os << "0"; // Special case for zero
         } else {
             // Use stringstream to easily remove leading zeros
             std::stringstream ss;
             ss << std::hex << low;
             os << ss.str();
         }
     }

     // Restore the original stream flags
     os.flags(old_flags);
     return os;
 }

 // Optional: Specialization for signed __int128 if needed
 // inline std::ostream&
 // operator<<(std::ostream& os, const __int128& value) { ... }

 } // namespace std


 // --- Provide std::hash specialization for __uint128_t ---
 namespace std {

 /**
  * @brief Specialization of std::hash for __uint128_t.
  *
  * Combines the hash of the high and low 64-bit parts
  * to produce a hash for the full 128-bit value.
  */
 template <>
 struct hash<__uint128_t>
 {
     using argument_type = __uint128_t; ///< Type of the input value
     using result_type   = size_t;           ///< Type of the hash result

     /**
      * @brief Calculates the hash of an __uint128_t value.
      * @param val The value to hash.
      * @return The hash value.
      */
     result_type operator()(const argument_type& val) const noexcept
     {
         // Decompose into high and low 64-bit parts
         const uint64_t low  = static_cast<uint64_t>(val);
         const uint64_t high = static_cast<uint64_t>(val >> 64);

         // Hash the individual parts using the standard hash for uint64_t
         const hash<uint64_t> hasher;
         const result_type hash_low  = hasher(low);
         const result_type hash_high = hasher(high);

         // Combine the two hash values.
         // A simple xor with shift is used here.
         // Other combination methods are possible.
         return hash_low ^ (hash_high << 1);
         // Alternative combination (similar to boost):
         // return hash_low + 0x9e3779b9ULL +
         // (hash_high << 6) + (hash_high >> 2);
     }
 };

 // Optional: Specialization for signed __int128 if needed
 // template <> struct hash<__int128> { ... };

 } // namespace std

 #endif // __SIZEOF_INT128__

 // If __int128 is not supported, this file essentially becomes empty.
 // This is safe as the features it
 // provides are only needed when __int128 is used.

 #endif // __BASE_INT128_SUPPORT_HH__
