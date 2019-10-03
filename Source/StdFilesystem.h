#ifndef __REDEFINE__STD_FILESYSTEM__
#define __REDEFINE__STD_FILESYSTEM__

#if defined (HAVE_FILESYSTEM)
# include <filesystem>
namespace std_filesystem = HAVE_FILESYSTEM_NAMESPACE;
#elif defined (HAVE_EXPERIMENTAL_FILESYSTEM)
# include <experimental/filesystem>
namespace std_filesystem = HAVE_EXPERIMENTAL_FILESYSTEM_NAMESPACE;
#endif

#endif // __REDEFINE__STD_FILESYSTEM__ //
