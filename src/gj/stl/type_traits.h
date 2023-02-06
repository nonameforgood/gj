#if !defined(NRF)

#include <type_traits>

#else

namespace std {

  /// integral_constant
  template<typename _Tp, _Tp __v>
    struct integral_constant_NONO
    {
      static constexpr _Tp                  value = __v;
      typedef _Tp                           value_type;
      //typedef integral_constant<_Tp, __v>   type;
      constexpr operator value_type() const noexcept { return value; }
#if __cplusplus > 201103L

//#define __cpp_lib_integral_constant_callable 201304

      constexpr value_type operator()() const noexcept { return value; }
#endif
    };

}
#endif