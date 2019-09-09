#ifndef __REDEFINE__STD_FILESYSTEM__
#define __REDEFINE__STD_FILESYSTEM__

#if defined (HAVE_FILESYSTEM)
# include <filesystem>
# if defined (_MSC_VER)
#  if _MSC_VER >= 1914 && _MSVC_LANG >= 201703L
namespace std_filesystem = std::filesystem;
#  elif _MSC_VER >= 1910
namespace std_filesystem = std::experimental::filesystem;
#  elif _MSC_VER >= 1700
namespace std_filesystem = std::tr2::sys;
#  else
#   error "std::filesystem"
#  endif
# else
namespace std_filesystem = std::filesystem;
# endif
#elif defined (HAVE_EXPERIMENTAL_FILESYSTEM)
# include <experimental/filesystem>
namespace std_filesystem = std::experimental::filesystem;
#else
# error "std::filesystem"
#endif

#endif // __REDEFINE__STD_FILESYSTEM__ //
