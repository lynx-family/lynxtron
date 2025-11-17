#ifndef SHELL_COMMON_PATH_PROVIDER_H_
#define SHELL_COMMON_PATH_PROVIDER_H_
namespace base {
class FilePath;
}
namespace lynxtron {
bool PathProvider(int key, base::FilePath* result);
}

#endif  // SHELL_COMMON_PATH_PROVIDER_H_
