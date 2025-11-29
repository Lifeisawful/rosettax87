#include <errno.h>
#include <fcntl.h>
#include <libproc.h>
#include <mach-o/dyld.h> // For _NSGetExecutablePath
#include <mach-o/dyld_images.h>
#include <mach/arm/thread_status.h>
#include <mach/error.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdint.h> // Add for uint64_t
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/stat.h> // For chmod
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <map>

#include "macho_loader.hpp"
#include "offset_finder.hpp"

// #define WINE

static bool g_is_wine = false;
static bool g_is_wine_preloader = false;

typedef const struct dyld_process_info_base *dyld_process_info;
extern "C" dyld_process_info
_dyld_process_info_create(task_t task, uint64_t timestamp,
                          kern_return_t *kernelError);

extern "C" void _dyld_process_info_for_each_image(
    dyld_process_info info,
    void (^callback)(uint64_t machHeaderAddress, const uuid_t uuid,
                     const char *path));

extern "C" void _dyld_process_info_release(dyld_process_info info);

class MuhDebugger {
private:
  pid_t childPid;
  task_t taskPort;
  static const unsigned int AARCH64_BREAKPOINT; // just declare here
  std::map<uint64_t, unsigned int> breakpoints; // addr -> original instruction
  static const int MAX_WATCHPOINTS =
      4; // AArch64 typically has 4 hardware watchpoints
  std::map<uint64_t, int> watchpoints; // address -> debug register index
  bool waitForEvent(int *status) {
    if (waitpid(childPid, status, 0) == -1) {
      perror("waitpid");
      return false;
    }
    if (WIFSTOPPED(*status)) {
      int signal = WSTOPSIG(*status);
      printf("Process stopped signal=%d\n", signal);

      if (signal == SIGBUS) {
        printf("accessing thread\n");
        arm_thread_state64_t thread_state;

        thread_act_port_array_t thread_list;
        mach_msg_type_number_t thread_count = 0; // Initialize to 0
        // Get thread port
        if (task_threads(taskPort, &thread_list, &thread_count) ==
            KERN_SUCCESS) {
          printf("task_threads succeeded, thread_count=%d\n", thread_count);

          // Get register state
          mach_msg_type_number_t state_count = ARM_THREAD_STATE64_COUNT;
          kern_return_t kr =
              thread_get_state(thread_list[0], ARM_THREAD_STATE64,
                               (thread_state_t)&thread_state, &state_count);
          if (kr == KERN_SUCCESS) {
            printf("Thread state:\n");
            uint64_t pc = thread_state.__pc;

            // Get VM region info
            vm_address_t region_address = pc;
            vm_size_t region_size;
            vm_region_basic_info_data_64_t info;
            mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object_name;

            if (vm_region_64(taskPort, &region_address, &region_size,
                             VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info,
                             &info_count, &object_name) == KERN_SUCCESS) {
              printf("SIGBUS Details:\n");
              printf("-> Fault Address: 0x%llx\n", pc);
              printf("-> Region Start: 0x%llx\n", region_address);
              printf("-> Region End: 0x%llx\n", region_address + region_size);
              printf("-> Region Size: 0x%llx\n", region_size);
              printf("-> Region Permissions: %c%c%c\n",
                     (info.protection & VM_PROT_READ) ? 'r' : '-',
                     (info.protection & VM_PROT_WRITE) ? 'w' : '-',
                     (info.protection & VM_PROT_EXECUTE) ? 'x' : '-');
              printf("-> Region Alignment: 0x%x\n",
                     (unsigned int)region_address % 16);

              // Print the instruction at the fault address
              uint32_t instruction;
              mach_vm_size_t size = sizeof(instruction);
              if (mach_vm_read_overwrite(taskPort, pc, size,
                                         (mach_vm_address_t)&instruction,
                                         &size) == KERN_SUCCESS) {
                printf("-> Instruction at Fault Address: 0x%x\n", instruction);
              } else {
                printf("-> Failed to read instruction at Fault Address\n");
              }
            }
          } else {
            printf("thread_get_state failed: %s\n", mach_error_string(kr));
          }
          mach_port_deallocate(mach_task_self(), thread_list[0]);
        } else {
          printf("task_threads failed\n");
        }
      }
    }
    return true;
  }

  const char *findModuleForAddress(uint64_t address) {
    kern_return_t kr;
    auto process_info = _dyld_process_info_create(taskPort, 0, &kr);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get dyld process info (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return NULL;
    }

    __block const char *modpath = nullptr;

    _dyld_process_info_for_each_image(
        process_info,
        ^(uint64_t machHeaderAddress, const uuid_t uuid, const char *path) {
          if (address == machHeaderAddress) {
            modpath = strdup(path);
          }
        });

    return modpath ? modpath : strdup("<unknown>");
  }

  bool configureDebugRegisters(thread_act_port_array_t thread_list) {
    arm_debug_state64_t debug_state;
    mach_msg_type_number_t count = ARM_DEBUG_STATE64_COUNT;

    kern_return_t kr = thread_get_state(thread_list[0], ARM_DEBUG_STATE64,
                                        (thread_state_t)&debug_state, &count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get debug state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    // Enable monitor mode in MDSCR
    debug_state.__mdscr_el1 |= 0x8000; // Set MDE bit (bit 15)

    kr =
        thread_set_state(thread_list[0], ARM_DEBUG_STATE64,
                         (thread_state_t)&debug_state, ARM_DEBUG_STATE64_COUNT);

    if (kr != KERN_SUCCESS) {
      printf("Failed to set debug state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    return true;
  }

public:
  bool adjustMemoryProtection(uint64_t address, vm_prot_t protection,
                              mach_vm_size_t size = 0) {
    long page_size = 0x1000;
    mach_vm_address_t region =
        address & ~(page_size - 1); // align to page boundary
    // align size to page boundary
    if (size == 0) {
      size = page_size;
    }
    size = (size + page_size - 1) & ~(page_size - 1);

    printf("Adjusting memory protection at 0x%llx - 0x%llx\n",
           (unsigned long long)region, (unsigned long long)(region + size));

    for (mach_vm_size_t offset = 0; offset < size; offset += page_size) {
      mach_vm_address_t page = region + offset;
      kern_return_t kr =
          mach_vm_protect(taskPort, page, page_size, FALSE, protection);
      if (kr != KERN_SUCCESS) {
        printf(
            "Failed to adjust memory protection at 0x%llx (error 0x%x: %s)\n",
            (unsigned long long)page, kr, mach_error_string(kr));
        return false;
      }
    }
    return true;
  }
  bool attach(pid_t pid) {
    childPid = pid;
    printf("attempting to attach to %d\n", childPid);
    if (ptrace(PT_ATTACH, childPid, 0, 0) < 0) {
      perror("ptrace(PT_ATTACHEXC)");
      return false;
    }

    // ::usleep(250000);

    printf("wait for event\n");
    int status;
    waitForEvent(&status);

    printf("continued execution\n");

    // Get task port for the child process
    if (task_for_pid(mach_task_self(), childPid, &taskPort) != KERN_SUCCESS) {
      printf("Failed to get task port for pid %d\n", childPid);
      return false;
    }

    auto attach_count = g_is_wine_preloader ? 2 : 1;

    for (auto i = 0; i < attach_count; i++) {

      printf("continuing..\n");
      if (!continueExecution()) {
        printf("Failed to continue execution\n");
        return false;
      }

      printf("waited..\n");
      // Get task port for the child process
      if (task_for_pid(mach_task_self(), childPid, &taskPort) != KERN_SUCCESS) {
        printf("Failed to get task port for pid %d\n", childPid);
        return false;
      }

      printf("acquired task port\n");
    }

    printf("Started debugging process %d\n", childPid);
    return true;
  }

  bool continueExecution() {
    if (ptrace(PT_CONTINUE, childPid, (caddr_t)1, 0) < 0) {
      perror("ptrace(PT_CONTINUE)");
      return false;
    }

    // usleep(250000);

    printf("continueExecution waiting for event ..\n");

    int status;
    return waitForEvent(&status);
  }

  bool singleStep() {
    if (ptrace(PT_STEP, childPid, (caddr_t)1, 0) < 0) {
      perror("ptrace(PT_STEP)");
      return false;
    }
    return true;
  }

  bool detach() {
    if (ptrace(PT_DETACH, childPid, (caddr_t)1, 0) < 0) {
      perror("ptrace(PT_DETACH)");
      return false;
    }
    return true;
  }

  struct ModuleInfo {
    uintptr_t address;
    std::string path;
  };

  auto getModuleList() -> std::vector<ModuleInfo> {
    __block std::vector<ModuleInfo> moduleList;
    kern_return_t kr;
    auto process_info = _dyld_process_info_create(taskPort, 0, &kr);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get dyld process info (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return moduleList;
    }

    _dyld_process_info_for_each_image(
        process_info, ^(uint64_t address, const uuid_t uuid, const char *path) {
          moduleList.push_back({address, std::string(path)});
        });

    return moduleList;
  }

  auto find_runtime() -> uintptr_t {
    auto module_list = getModuleList();

    auto runtime_it = std::find_if(
        module_list.begin(), module_list.end(), [](const ModuleInfo &module) {
          return module.path == "/usr/libexec/rosetta/runtime";
        });
    if (runtime_it != module_list.end()) {
      return runtime_it->address;
    }

    mach_vm_address_t address = 0;
    mach_vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    while (true) {
      if (mach_vm_region(taskPort, &address, &size, VM_REGION_BASIC_INFO_64,
                         (vm_region_info_t)&info, &count,
                         &object_name) != KERN_SUCCESS) {
        break;
      }

      if (info.protection & (VM_PROT_EXECUTE | VM_PROT_READ)) {
        if (std::find_if(module_list.begin(), module_list.end(),
                         [address](const ModuleInfo &module) {
                           return address == module.address;
                         }) == module_list.end()) {
          uint32_t magic_bytes;
          if (readMemory(address, &magic_bytes, sizeof(magic_bytes)) &&
              magic_bytes == MH_MAGIC_64) {
            return address;
          }
        }
      }

      address += size;
    }

    return 0;
  }

  uint64_t findModule(const char *moduleName) {
    kern_return_t kr;
    auto process_info = _dyld_process_info_create(taskPort, 0, &kr);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get dyld process info (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return 0;
    }

    __block uint64_t machHeaderAddress = 0;

    _dyld_process_info_for_each_image(
        process_info, ^(uint64_t address, const uuid_t uuid, const char *path) {
          if (strstr(path, moduleName)) {
            machHeaderAddress = address;
          }
        });

    return machHeaderAddress;
  }

  bool setBreakpoint(uint64_t address) {
    // Verify address is in valid range
    if (address >= MACH_VM_MAX_ADDRESS) {
      printf("Invalid address 0x%llx\n", (unsigned long long)address);
      return false;
    }
    unsigned int original;
    mach_vm_size_t read_size;

    // Read the original instruction
    kern_return_t kr =
        mach_vm_read_overwrite(taskPort, address, sizeof(unsigned int),
                               (mach_vm_address_t)&original, &read_size);
    if (kr != KERN_SUCCESS) {
      printf("Failed to read memory at 0x%llx (error 0x%x: %s)\n",
             (unsigned long long)address, kr, mach_error_string(kr));
      return false;
    }

    // printf("read success\n");
    // First, try to adjust memory protection
    if (!adjustMemoryProtection(address,
                                VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY)) {
      return false;
    }

    // printf("adjustMemoryProtection success\n");

    // Write breakpoint instruction
    kr = mach_vm_write(taskPort, address, (vm_offset_t)&AARCH64_BREAKPOINT,
                       sizeof(unsigned int));
    if (kr != KERN_SUCCESS) {
      printf("Failed to write breakpoint at 0x%llx (error 0x%x: %s)\n",
             (unsigned long long)address, kr, mach_error_string(kr));
      return false;
    }
    // printf("write success\n");
    if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_EXECUTE)) {
      return false;
    }

    // printf("adjustMemoryProtection success\n");
    breakpoints[address] = original;
    printf("Breakpoint set at address 0x%llx\n", (unsigned long long)address);
    return true;
  }

  bool removeBreakpoint(uint64_t address) {
    auto it = breakpoints.find(address);
    if (it == breakpoints.end()) {
      printf("No breakpoint found at address 0x%llx\n",
             (unsigned long long)address);
      return false;
    }

    // First, try to adjust memory protection
    if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_WRITE)) {
      return false;
    }

    // Restore original instruction
    kern_return_t kr = mach_vm_write(
        taskPort, address, (vm_offset_t)&it->second, sizeof(unsigned int));
    if (kr != KERN_SUCCESS) {
      printf(
          "Failed to restore original instruction at 0x%llx (error 0x%x: %s)\n",
          (unsigned long long)address, kr, mach_error_string(kr));
      return false;
    }
    if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_EXECUTE)) {
      return false;
    }
    breakpoints.erase(it);
    printf("Breakpoint removed from address 0x%llx\n",
           (unsigned long long)address);
    return true;
  }

  bool isBreakpoint(uint64_t address) {
    return breakpoints.find(address) != breakpoints.end();
  }

  bool listModules() {
    task_t task;
    if (task_for_pid(mach_task_self(), childPid, &task) != KERN_SUCCESS) {
      printf("Failed to get task for pid %d\n", childPid);
      return false;
    }

    mach_vm_address_t address = 0;
    mach_vm_size_t size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    printf("Memory regions:\n");
    while (true) {
      if (mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO_64,
                         (vm_region_info_t)&info, &count,
                         &object_name) != KERN_SUCCESS) {
        break;
      }

      char permissions[4] = {(info.protection & VM_PROT_READ) ? 'r' : '-',
                             (info.protection & VM_PROT_WRITE) ? 'w' : '-',
                             (info.protection & VM_PROT_EXECUTE) ? 'x' : '-',
                             '\0'};

      const char *module = findModuleForAddress(address);
      printf("0x%llx - 0x%llx %s %s\n", (unsigned long long)address,
             (unsigned long long)(address + size), permissions,
             module ? module : "<unknown>");
      free((void *)module);

      address += size;
    }

    mach_port_deallocate(mach_task_self(), task);
    return true;
  }

  bool printRegisters() {
    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    kern_return_t kr = task_threads(taskPort, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
      printf("Failed to get threads (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    // We'll use the first thread
    arm_thread_state64_t state;
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    kr = thread_get_state(thread_list[0], ARM_THREAD_STATE64,
                          (thread_state_t)&state, &count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get thread state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    // Print x0 through x28
    for (int i = 0; i <= 28; i++) {
      printf("x%-2d = 0x%016llx\n", i, state.__x[i]);
    }

    // Print special registers
    printf("fp  = 0x%016llx\n", state.__fp);
    printf("lr  = 0x%016llx\n", state.__lr);
    printf("sp  = 0x%016llx\n", state.__sp);
    printf("pc  = 0x%016llx\n", state.__pc);
    printf("cpsr= 0x%08x\n", state.__cpsr);

    // Cleanup
    for (unsigned int i = 0; i < thread_count; i++) {
      mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_t) * thread_count);

    return true;
  }

  bool printMemory(uint64_t address, size_t size) {
    vm_offset_t buffer;
    mach_msg_type_number_t read_size = (mach_msg_type_number_t)size;

    kern_return_t kr =
        mach_vm_read(taskPort, address, size, &buffer, &read_size);

    if (kr != KERN_SUCCESS) {
      printf("Failed to read memory at 0x%llx (error 0x%x: %s)\n",
             (unsigned long long)address, kr, mach_error_string(kr));
      return false;
    }

    // Print hexdump
    uint8_t *data = (uint8_t *)buffer;
    for (size_t i = 0; i < read_size; i += 16) {
      // Print address
      printf("%016llx: ", (unsigned long long)(address + i));

      // Print hex bytes
      for (size_t j = 0; j < 16; j++) {
        if (i + j < read_size) {
          printf("%02x ", data[i + j]);
        } else {
          printf("   ");
        }
        if (j == 7)
          printf(" "); // Extra space between groups of 8
      }

      // Print ASCII representation
      printf(" |");
      for (size_t j = 0; j < 16; j++) {
        if (i + j < read_size) {
          uint8_t c = data[i + j];
          printf("%c", (c >= 32 && c <= 126) ? c : '.');
        } else {
          printf(" ");
        }
      }
      printf("|\n");
    }

    // Deallocate the buffer
    vm_deallocate(mach_task_self(), buffer, read_size);
    return true;
  }

  // Add this enum inside the class
  enum Register {
    X0,
    X1,
    X2,
    X3,
    X4,
    X5,
    X6,
    X7,
    X8,
    X9,
    X10,
    X11,
    X12,
    X13,
    X14,
    X15,
    X16,
    X17,
    X18,
    X19,
    X20,
    X21,
    X22,
    X23,
    X24,
    X25,
    X26,
    X27,
    X28,
    FP,
    LR,
    SP,
    PC,
    CPSR
  };

  uint64_t readRegister(Register reg) {
    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    kern_return_t kr = task_threads(taskPort, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
      printf("Failed to get threads (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return 0;
    }

    arm_thread_state64_t state;
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    kr = thread_get_state(thread_list[0], ARM_THREAD_STATE64,
                          (thread_state_t)&state, &count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get thread state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return 0;
    }

    uint64_t value = 0;
    if (reg >= X0 && reg <= X28) {
      value = state.__x[reg];
    } else {
      switch (reg) {
      case FP:
        value = state.__fp;
        break;
      case LR:
        value = state.__lr;
        break;
      case SP:
        value = state.__sp;
        break;
      case PC:
        value = state.__pc;
        break;
      case CPSR:
        value = state.__cpsr;
        break;
      default: {
        printf("Invalid register\n");
        return 0;
      }
      }
    }

    // Cleanup
    for (unsigned int i = 0; i < thread_count; i++) {
      mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_t) * thread_count);

    return value;
  }

  bool setRegister(Register reg, uint64_t value) {
    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    kern_return_t kr = task_threads(taskPort, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
      printf("Failed to get threads (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    arm_thread_state64_t state;
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    kr = thread_get_state(thread_list[0], ARM_THREAD_STATE64,
                          (thread_state_t)&state, &count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get thread state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    if (reg >= X0 && reg <= X28) {
      state.__x[reg] = value;
    } else {
      switch (reg) {
      case FP:
        state.__fp = value;
        break;
      case LR:
        state.__lr = value;
        break;
      case SP:
        state.__sp = value;
        break;
      case PC:
        state.__pc = value;
        break;
      case CPSR:
        state.__cpsr = value;
        break;
      default: {
        printf("Invalid register\n");
        return false;
      }
      }
    }

    kr = thread_set_state(thread_list[0], ARM_THREAD_STATE64,
                          (thread_state_t)&state, ARM_THREAD_STATE64_COUNT);
    if (kr != KERN_SUCCESS) {
      printf("Failed to set thread state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    // Cleanup
    for (unsigned int i = 0; i < thread_count; i++) {
      mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_t) * thread_count);

    return true;
  }

  bool readMemory(uint64_t address, void *buffer, size_t size) {
    mach_vm_size_t read_size;

    kern_return_t kr = mach_vm_read_overwrite(
        taskPort, address, size, (mach_vm_address_t)buffer, &read_size);

    if (kr != KERN_SUCCESS) {
      printf("Failed to read memory at 0x%llx (error 0x%x: %s)\n",
             (unsigned long long)address, kr, mach_error_string(kr));
      return false;
    }

    return read_size == size;
  }

  bool writeMemory(uint64_t address, const void *buffer, size_t size) {
    kern_return_t kr =
        mach_vm_write(taskPort, address, (vm_offset_t)buffer, size);

    if (kr != KERN_SUCCESS) {
      printf("Failed to write memory at 0x%llx (error 0x%x: %s)\n",
             (unsigned long long)address, kr, mach_error_string(kr));
      return false;
    }

    return true;
  }

  uint64_t allocateMemory(size_t size) {
    mach_vm_address_t address = 0; // Let system choose the address

    kern_return_t kr =
        mach_vm_allocate(taskPort, &address, size, VM_FLAGS_ANYWHERE);

    if (kr != KERN_SUCCESS) {
      printf("Failed to allocate memory (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return 0;
    }

    // Set memory protection to RWX
    if (!adjustMemoryProtection(address, VM_PROT_READ | VM_PROT_WRITE)) {
      // If protection fails, deallocate the memory
      mach_vm_deallocate(taskPort, address, size);
      return 0;
    }

    printf("Allocated %zu bytes at 0x%llx\n", size,
           (unsigned long long)address);
    return address;
  }

  bool copyThreadState(arm_thread_state64_t &state) {
    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    kern_return_t kr = task_threads(taskPort, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
      printf("Failed to get threads (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    kr = thread_get_state(thread_list[0], ARM_THREAD_STATE64,
                          (thread_state_t)&state, &count);

    // Cleanup
    for (unsigned int i = 0; i < thread_count; i++) {
      mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_t) * thread_count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get thread state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    return true;
  }

  bool restoreThreadState(const arm_thread_state64_t &state) {
    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    kern_return_t kr = task_threads(taskPort, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
      printf("Failed to get threads (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    kr = thread_set_state(thread_list[0], ARM_THREAD_STATE64,
                          (thread_state_t)&state, ARM_THREAD_STATE64_COUNT);

    // Cleanup
    for (unsigned int i = 0; i < thread_count; i++) {
      mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_t) * thread_count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to set thread state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    return true;
  }

  bool printStackTrace() {
    arm_thread_state64_t state;
    if (!copyThreadState(state)) {
      return false;
    }

    uint64_t fp = state.__fp;
    uint64_t lr = state.__lr;
    uint64_t pc = state.__pc;

    printf("Stack trace:\n");
    printf("#0 0x%llx\n", pc);

    int frame = 1;
    // Follow frame pointers until we hit NULL or an invalid address
    while (fp != 0 &&
           frame < 32) { // Limit to 32 frames to prevent infinite loops
      uint64_t prev_fp;
      uint64_t prev_lr;

      // Read previous frame pointer and link register
      if (!readMemory(fp, &prev_fp, sizeof(prev_fp)) ||
          !readMemory(fp + 8, &prev_lr, sizeof(prev_lr))) {
        printf("Failed to read stack frame at 0x%llx\n", fp);
        break;
      }

      // Print the frame
      const char *module = findModuleForAddress(prev_lr);
      printf("#%d 0x%llx %s\n", frame, prev_lr, module);
      free((void *)module);

      // Move to previous frame
      fp = prev_fp;
      frame++;
    }

    return true;
  }

  bool setWatchpoint(uint64_t address, size_t size) {
    if (watchpoints.size() >= MAX_WATCHPOINTS) {
      printf("No available hardware watchpoints\n");
      return false;
    }

    // Size must be 1, 2, 4, or 8 bytes and address must be aligned
    if (size != 1 && size != 2 && size != 4 && size != 8) {
      printf("Invalid watchpoint size (must be 1, 2, 4, or 8)\n");
      return false;
    }

    if (address & (size - 1)) {
      printf("Address must be aligned to size\n");
      return false;
    }

    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    kern_return_t kr = task_threads(taskPort, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
      printf("Failed to get threads (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    arm_debug_state64_t debug_state;
    mach_msg_type_number_t count = ARM_DEBUG_STATE64_COUNT;

    kr = thread_get_state(thread_list[0], ARM_DEBUG_STATE64,
                          (thread_state_t)&debug_state, &count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get debug state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    // Find first free debug register
    int reg_idx = -1;
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
      if ((debug_state.__wcr[i] & 1) == 0) { // Check if watchpoint is disabled
        reg_idx = i;
        break;
      }
    }

    if (reg_idx == -1) {
      printf("No available hardware watchpoints\n");
      return false;
    }

    // Configure watchpoint
    debug_state.__wvr[reg_idx] = address;

    // WCR bits:
    // Bit 0: Enable
    // Bits 1-3: Size encoding (001=1 byte, 010=2 bytes, 011=4 bytes, 100=8
    // bytes) Bits 5-8: Access type (10 = write) Bits 20-28: Length mask
    uint64_t wcr = 1; // Enable bit
    switch (size) {
    case 1:
      wcr |= (1ULL << 1);
      break;
    case 2:
      wcr |= (2ULL << 1);
      break;
    case 4:
      wcr |= (3ULL << 1);
      break;
    case 8:
      wcr |= (4ULL << 1);
      break;
    }
    wcr |= (2ULL << 5); // Write access

    debug_state.__wcr[reg_idx] = wcr;

    // Enable monitor mode if not already enabled
    if ((debug_state.__mdscr_el1 & 0x8000) == 0) {
      debug_state.__mdscr_el1 |= 0x8000;
    }

    kr =
        thread_set_state(thread_list[0], ARM_DEBUG_STATE64,
                         (thread_state_t)&debug_state, ARM_DEBUG_STATE64_COUNT);

    if (kr != KERN_SUCCESS) {
      printf("Failed to set debug state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    watchpoints[address] = reg_idx;
    printf("Watchpoint set at address 0x%llx (size=%zu)\n", address, size);

    // Cleanup
    for (unsigned int i = 0; i < thread_count; i++) {
      mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_t) * thread_count);

    return true;
  }

  bool removeWatchpoint(uint64_t address) {
    auto it = watchpoints.find(address);
    if (it == watchpoints.end()) {
      printf("No watchpoint found at address 0x%llx\n", address);
      return false;
    }

    thread_act_port_array_t thread_list;
    mach_msg_type_number_t thread_count;

    kern_return_t kr = task_threads(taskPort, &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
      printf("Failed to get threads (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    arm_debug_state64_t debug_state;
    mach_msg_type_number_t count = ARM_DEBUG_STATE64_COUNT;

    kr = thread_get_state(thread_list[0], ARM_DEBUG_STATE64,
                          (thread_state_t)&debug_state, &count);

    if (kr != KERN_SUCCESS) {
      printf("Failed to get debug state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    // Disable the watchpoint
    debug_state.__wcr[it->second] = 0;

    kr =
        thread_set_state(thread_list[0], ARM_DEBUG_STATE64,
                         (thread_state_t)&debug_state, ARM_DEBUG_STATE64_COUNT);

    if (kr != KERN_SUCCESS) {
      printf("Failed to set debug state (error 0x%x: %s)\n", kr,
             mach_error_string(kr));
      return false;
    }

    watchpoints.erase(it);
    printf("Watchpoint removed from address 0x%llx\n", address);

    // Cleanup
    for (unsigned int i = 0; i < thread_count; i++) {
      mach_port_deallocate(mach_task_self(), thread_list[i]);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  sizeof(thread_t) * thread_count);

    return true;
  }

  ~MuhDebugger() {
    if (taskPort != MACH_PORT_NULL) {
      mach_port_deallocate(mach_task_self(), taskPort);
    }
  }
};

// Define the static constant outside the class
const unsigned int MuhDebugger::AARCH64_BREAKPOINT = 0xD4200000;

struct Exports {
  uint64_t version; // 0x1560000000000
  uint64_t x87_exports;
  uint64_t x87_export_count;
  uint64_t runtime_exports;
  uint64_t runtime_export_count;
};

struct Export {
  uint64_t address;
  uint64_t name;
};

int run_helper_mode() {
  const char *socket_path = "/var/run/rosetta_helper.sock";

  // Create Unix domain socket
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  // Remove existing socket if present
  unlink(socket_path);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  // Set socket permissions to allow non-root users to connect
  chmod(socket_path, 0666);

  if (listen(server_fd, 5) < 0) {
    perror("listen");
    close(server_fd);
    return 1;
  }

  printf("Helper listening on %s\n", socket_path);

  while (true) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
      perror("accept");
      continue;
    }

    // Read PID from client
    pid_t target_pid;
    ssize_t bytes_read = recv(client_fd, &target_pid, sizeof(target_pid), 0);
    if (bytes_read != sizeof(target_pid)) {
      perror("recv");
      close(client_fd);
      continue;
    }

    printf("Received request to debug PID: %d\n", target_pid);

    // Fork and execute new instance
    pid_t child = fork();
    if (child == 0) {
      // Child process
      char pid_str[32];
      snprintf(pid_str, sizeof(pid_str), "%d", target_pid);

      // Get path to current executable
      char path[4096];
      uint32_t path_size = sizeof(path);
      if (_NSGetExecutablePath(path, &path_size) == 0) {
        char *args[] = {path, pid_str, NULL};
        execv(path, args);
        perror("execv");
      }
      exit(1);
    }

    // Send success response
    uint8_t success = 1;
    send(client_fd, &success, sizeof(success), 0);
    close(client_fd);
  }

  close(server_fd);
  return 0;
}
std::string get_process_name(int pid) {
  char name[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_name(pid, name, sizeof(name)) <= 0) {
    return ""; // Empty string if failed
  }
  return std::string(name);
}
#include <sys/sysctl.h>

std::string get_process_cmdline(int pid) {
  int mib[4] = {CTL_KERN, KERN_PROCARGS2, pid, 0};
  size_t size = 0;

  // Get buffer size
  if (sysctl(mib, 3, NULL, &size, NULL, 0) < 0) {
    return "";
  }

  // Allocate buffer
  char *buffer = (char *)malloc(size);
  if (!buffer) {
    return "";
  }

  // Get process args
  if (sysctl(mib, 3, buffer, &size, NULL, 0) < 0) {
    free(buffer);
    return "";
  }

  // First int in buffer is argc
  int argc = *(int *)buffer;
  printf("process argc count: %d\n", argc);

  // Skip argc and executable path
  char *p = buffer + sizeof(int);
  while (*p != '\0')
    p++;
  p++;

  while (*p == '\0')
    p++;

  // Build command line string
  std::string cmdline;
  for (int i = 0; i < argc && *p != '\0'; i++) {
    if (i > 0)
      cmdline += " ";
    cmdline += p;
    while (*p != '\0')
      p++;
    p++;
  }

  free(buffer);
  return cmdline;
}

int main(int argc, char *argv[]) {

  // Set up offsets dynamically
  OffsetFinder offset_finder;
  // Set default offsets temporarily (or just in case we need to fall back)
  offset_finder.set_default_offsets();
  // Search the rosetta runtime binary for offsets.
  offset_finder.determine_offsets();

  if (geteuid() != 0) {
    if (argc < 2) {
      printf("Usage: %s <program_to_debug>\n", argv[0]);
      return 1;
    }
    // connect to helper tool socket and send PID
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
      perror("socket");
      return 1;
    }

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/var/run/rosetta_helper.sock",
            sizeof(addr.sun_path) - 1);

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("connect");
      close(client_fd);
      return 1;
    }

    pid_t pid = getpid();

    ssize_t bytes_sent = send(client_fd, &pid, sizeof(pid), 0);
    if (bytes_sent != sizeof(pid)) {
      perror("send");
      close(client_fd);
      return 1;
    }

    uint8_t success;

    ssize_t bytes_read = recv(client_fd, &success, sizeof(success), 0);

    if (bytes_read != sizeof(success)) {
      perror("recv");
      close(client_fd);
      return 1;
    }

    close(client_fd);
    // Non-root code path
    // raise(SIGSTOP);
    usleep(50000000);
    printf("launching into program\n");
    execv(argv[1], &argv[1]);
    return 1;
  }

  // Root code path
  if (argc < 2) {
    // Run as helper tool
    return run_helper_mode();
  }

  // Debug specific process
  int pid = atoi(argv[1]);
  printf("Running as root, attaching to process %d\n", pid);

  MuhDebugger dbg;

  auto cmdline = get_process_cmdline(pid);
  printf("cmdline: %s\n", cmdline.c_str());
  if (cmdline.find("wine") != std::string::npos) {
    printf("Wine process detected\n");
    g_is_wine = true;
  }

  if (!dbg.attach(pid)) {
    printf("Failed to attach to process\n");
    return 1;
  }
  printf("Attached successfully\n");

  auto module_list = dbg.getModuleList();

  for (const auto &module : module_list) {
    printf("address %llx, name %s\n", module.address, module.path.c_str());
  }

  const auto runtime_base = dbg.find_runtime();

  printf("Rosetta runtime base: 0x%llx\n", runtime_base);

  if (runtime_base == 0) {
    printf("Failed to find Rosetta runtime\n");
    return 1;
  }

  dbg.setBreakpoint(runtime_base + offset_finder.offset_loop_copy_func);
  dbg.continueExecution();

#if 0
  // Uncomment to enable debug flags in Rosetta runtime, not recommended
  // enable PRINT_IR
  // dbg.writeMemory(runtime_base + 0x3B280, "\x01", 1);
  
  // enable PRINT_SEGMENTS
  // dbg.writeMemory(runtime_base + 0x3B281, "\x01", 1);
  
  // enable DISABLE_EXCEPTIONS
  // dbg.writeMemory(runtime_base + 0x3B27D, "\x01", 1);
  
  // enable DISABLE_SIGACTION
  // dbg.writeMemory(runtime_base + 0x3B27E, "\x01", 1);
  
  // enable ALLOW_GUARD_PAGES
  // dbg.writeMemory(runtime_base + 0x3B278, "\x01", 1);
  
  // // enable SCRIBBLE_TRANSLATIONS
  // dbg.writeMemory(runtime_base + 0x3B282, "\x01", 1);
  
  // // enable AOT_ERRORS_ARE_FATAL
  // dbg.writeMemory(runtime_base + 0x3B279, "\x01", 1);
#endif

  auto rosetta_runtime_exports_address =
      dbg.readRegister(MuhDebugger::Register::X19);
  printf("Rosetta runtime exports: 0x%llx\n", rosetta_runtime_exports_address);

  Exports exports;
  dbg.readMemory(rosetta_runtime_exports_address, &exports, sizeof(exports));

  printf("Rosetta version: %llx\n", exports.version);

  char path[PATH_MAX];
  uint32_t path_size = sizeof(path);
  if (_NSGetExecutablePath(path, &path_size) != 0) {
    printf("Failed to get executable path\n");
    return 1;
  }

  // get the directory of the current executable
  std::filesystem::path executable_path(path);
  std::filesystem::path executable_dir = executable_path.parent_path();

  MachoLoader macho_loader;

  if (!macho_loader.open(executable_dir / "libRuntimeRosettax87")) {
    printf("Failed to open Mach-O file\n");
    return 1;
  }

  // we need to call mmap to allocate the memory for our macho

  uint64_t macho_base = 0; // dbg.allocateMemory(macho_loader.image_size());

  // first we store the original state of the thread
  arm_thread_state64_t backup_thread_state;
  dbg.copyThreadState(backup_thread_state);

  // setup a breakpoint after mmap syscall
  dbg.setBreakpoint(runtime_base + offset_finder.offset_svc_call_ret);

  // now we prepare the registers for the mmap call
  arm_thread_state64_t mmap_thread_state;
  memcpy(&mmap_thread_state, &backup_thread_state,
         sizeof(arm_thread_state64_t));

#define MAP_TRANSLATED_ALLOW_EXECUTE                                           \
  0x20000 /* allow execute in translated processes */

  mmap_thread_state.__x[0] = 0x210000000LL;                // addr
  mmap_thread_state.__x[1] = macho_loader.image_size();    // size
  mmap_thread_state.__x[2] = VM_PROT_READ | VM_PROT_WRITE; // prot
  mmap_thread_state.__x[3] =
      MAP_ANON | MAP_FIXED | MAP_TRANSLATED_ALLOW_EXECUTE; // flags
  mmap_thread_state.__x[4] = -1;                           // fd
  mmap_thread_state.__x[5] = 0;                            // offset

  mmap_thread_state.__pc = runtime_base + offset_finder.offset_svc_call_entry;

  dbg.restoreThreadState(mmap_thread_state);
  dbg.continueExecution();

  macho_base = dbg.readRegister(MuhDebugger::Register::X0);

  printf("Allocated memory at 0x%llx\n", macho_base);

  dbg.removeBreakpoint(runtime_base + offset_finder.offset_svc_call_ret);
  dbg.restoreThreadState(backup_thread_state);

  macho_loader.for_each_segment([&](segment_command_64 *segm) {
    auto dest = macho_base + segm->vmaddr;
    auto size = segm->vmsize;
    auto src = macho_loader.buffer_.data() + segm->fileoff;

    printf("Copying segment %s from 0x%llx to 0x%llx (%zx bytes)\n",
           segm->segname, (unsigned long long)segm->fileoff,
           (unsigned long long)dest, (unsigned long)size);

    dbg.writeMemory(dest, src, size);

    dbg.adjustMemoryProtection(dest, segm->initprot, segm->vmsize);
  });

  // fix up Exports segment of mapped macho
  uint64_t macho_exports_address =
      macho_base + macho_loader.get_section("__DATA", "exports")->addr;
  Exports macho_exports;

  dbg.readMemory(macho_exports_address, &macho_exports, sizeof(macho_exports));
  macho_exports.x87_exports += macho_base;
  macho_exports.runtime_exports += macho_base;

  std::vector<Export> x87_exports(macho_exports.x87_export_count);
  std::vector<Export> runtime_exports(macho_exports.runtime_export_count);

  dbg.readMemory(macho_exports.x87_exports, x87_exports.data(),
                 x87_exports.size() * sizeof(Export));
  dbg.readMemory(macho_exports.runtime_exports, runtime_exports.data(),
                 runtime_exports.size() * sizeof(Export));

  for (auto &exp : x87_exports) {
    exp.address += macho_base;
    exp.name += macho_base;
  }

  for (auto &exp : runtime_exports) {
    exp.address += macho_base;
    exp.name += macho_base;
  }

  dbg.writeMemory(macho_exports.x87_exports, x87_exports.data(),
                  x87_exports.size() * sizeof(Export));
  dbg.writeMemory(macho_exports.runtime_exports, runtime_exports.data(),
                  runtime_exports.size() * sizeof(Export));

  printf("macho_exports_address: 0x%llx\n", macho_exports_address);
  printf("macho_exports.x87_exports: 0x%llx\n", macho_exports.x87_exports);
  printf("macho_exports.runtime_exports: 0x%llx\n",
         macho_exports.runtime_exports);

  dbg.writeMemory(macho_exports_address, &macho_exports, sizeof(macho_exports));

  // look up imports section of mapped macho
  auto macho_imports_address =
      macho_base + macho_loader.get_section("__DATA", "imports")->addr;
  printf("macho_imports_address: 0x%llx\n", macho_imports_address);

  // read the exports from X19 register and copy them to the imports section of
  // the mapped macho
  auto lib_rosetta_runtime_exports_address =
      dbg.readRegister(MuhDebugger::Register::X19);
  printf("lib_rosetta_runtime_exports_address: 0x%llx\n",
         lib_rosetta_runtime_exports_address);

  Exports lib_rosetta_runtime_exports;
  dbg.readMemory(lib_rosetta_runtime_exports_address,
                 &lib_rosetta_runtime_exports,
                 sizeof(lib_rosetta_runtime_exports));

  printf("lib_rosetta_runtime_exports.version = 0x%llx\n",
         lib_rosetta_runtime_exports.version);
  printf("lib_rosetta_runtime_exports.x87_exports = 0x%llx\n",
         lib_rosetta_runtime_exports.x87_exports);
  printf("lib_rosetta_runtime_exports.x87_export_count = 0x%llx\n",
         lib_rosetta_runtime_exports.x87_export_count);
  printf("lib_rosetta_runtime_exports.runtime_exports = 0x%llx\n",
         lib_rosetta_runtime_exports.runtime_exports);
  printf("lib_rosetta_runtime_exports.runtime_export_count = 0x%llx\n",
         lib_rosetta_runtime_exports.runtime_export_count);

  dbg.writeMemory(macho_imports_address, &lib_rosetta_runtime_exports,
                  sizeof(lib_rosetta_runtime_exports));

  dbg.removeBreakpoint(runtime_base + offset_finder.offset_loop_copy_func);

  // replace the exports in X19 register with the address of the mapped macho
  dbg.setRegister(MuhDebugger::Register::X19, macho_exports_address);

  if (g_is_wine)
    dbg.detach();
  else {
    dbg.continueExecution();
  }

#if 0
  // Basic debugging loop example
  printf("Debugger attached. Commands:\n");
  printf("b <addr> - Set breakpoint at address\n");
  printf("r <addr> - Remove breakpoint at address\n");
  printf("c - Continue execution\n");
  printf("m - List mapped modules\n");
  printf("p - Print CPU registers\n");
  printf("x <addr> <size> - Examine memory\n");
  printf("q - Quit\n");

  char cmd;
  uint64_t addr;
  size_t size;
  int status;
  while (true) {
    printf("dbg> ");
    scanf(" %c", &cmd);

    switch (cmd) {
      case 'b':
        scanf("%llx", &addr);
        dbg.setBreakpoint(addr);
        break;
      case 'r':
        scanf("%llx", &addr);
        dbg.removeBreakpoint(addr);
        break;
      case 'c':
        dbg.continueExecution();
        break;
      case 'm':
        dbg.listModules();
        break;
      case 'p':
        dbg.printRegisters();
        break;
      case 't':
        dbg.printStackTrace();
        break;
      case 's':
        dbg.singleStep();
        break;
      case 'w': {
        scanf("%llx %zx", &addr, &size);
        dbg.setWatchpoint(addr, size);
      } break;
      case 'x':
        scanf("%llx %zx", &addr, &size);
        dbg.printMemory(addr, size);
        break;
      case 'q':
        return 0;
    }
  }
#endif
  return 0;
}
