// See LICENSE for license details.

#ifndef __HTIF_H
#define __HTIF_H

#include "memif.h"
#include "syscall.h"
#include "device.h"
#include <string.h>
#include <map>
#include <unordered_map>
#include <vector>

class htif_t : public chunked_memif_t
{
 public:
  htif_t();
  htif_t(int argc, char** argv);
  htif_t(const std::vector<std::string>& args);
  virtual ~htif_t();

  virtual void start();
  virtual void stop();

  int run();
  bool done();
  int exit_code();

  virtual memif_t& memif() { return mem; }

 protected:
  virtual void reset() = 0;
  // (modified 4)
  virtual void set_rom(bus_t &bus);

  // (modified )
  virtual void load_rom();

  // (modified 5) , use chip_config option to make dtb
  virtual void make_dtb(reg_t initrd_start, reg_t initrd_end, const char *bootargs);

  // (modified 7) 
  


  virtual void read_chunk(addr_t taddr, size_t len, void* dst) = 0;
  virtual void write_chunk(addr_t taddr, size_t len, const void* src) = 0;
  virtual void clear_chunk(addr_t taddr, size_t len);

  virtual size_t chunk_align() = 0;
  virtual size_t chunk_max_size() = 0;

  virtual std::map<std::string, uint64_t> load_payload(const std::string& payload, reg_t* entry);
  virtual void load_program();
  virtual void idle() {}

  const std::vector<std::string>& host_args() { return hargs; }

  reg_t get_entry_point() { return entry; }

  // indicates that the initial program load can skip writing this address
  // range to memory, because it has already been loaded through a sideband
  virtual bool is_address_preloaded(addr_t taddr, size_t len) { return false; }

  // THE TRANSPLANT INTERFACE (modified)
  std::vector<processor_t*> procs; // (modified 5) protected
  size_t freq;
  std::string htif_isa;
  std::unique_ptr<rom_device_t> boot_rom; // (modified 7) the make rom
  std::vector<std::pair<reg_t, mem_t*>> htif_mems;
  std::string dtb;
  reg_t start_pc;
  reg_t rst_vec;
 private:
  void parse_arguments(int argc, char ** argv);
  void register_devices();
  void usage(const char * program_name);

  //(modified 5)
  void make_dtb(reg_t initrd_start, reg_t initrd_end, const char* bootargs);
  //(modified 6)
  void chip_config();
  //(modified 7)
  void mems_config();
  //(modified 8)
  void make_flash_addr();
  void chrome_rom();

  void normal_load();
  void load_file();

  memif_t mem;
  reg_t entry;
  bool writezeros;
  std::vector<std::string> hargs;
  std::vector<std::string> targs;
  std::vector<std::string> plus_plus_load;
  std::unordered_map<std::string,std::string> argmap;//(modified 2)
  std::string sig_file;
  addr_t sig_addr; // torture
  addr_t sig_len; // torture
  addr_t tohost_addr;
  addr_t fromhost_addr;
  int exitcode;
  bool stopped;

  device_list_t device_list;
  syscall_t syscall_proxy;
  bcd_t bcd;
  std::vector<device_t*> dynamic_devices;
  std::vector<std::string> payloads;

  const std::vector<std::string>& target_args() { return targs; }

  friend class memif_t;
  friend class syscall_t;
};

static std::vector<std::pair<reg_t, mem_t*>> htif_helper_make_mems(const char* arg)
{
  // handle legacy mem argument
  char* p;
  auto mb = strtoull(arg, &p, 0);
  if (*p == 0) {
    reg_t size = reg_t(mb) << 20;
    if (size != (size_t)size)
      throw std::runtime_error("Size would overflow size_t");
    return std::vector<std::pair<reg_t, mem_t*>>(1, std::make_pair(reg_t(DRAM_BASE), new mem_t(size)));
  }

  // handle base/size tuples
  std::vector<std::pair<reg_t, mem_t*>> res;
  while (true) {
    auto base = strtoull(arg, &p, 0);
    if (!*p || *p != ':') {
      printf("modified 7: err parsing, expecting :\n");
      assert(0);
    }
    auto size = strtoull(p + 1, &p, 0);

    // page-align base and size
    auto base0 = base, size0 = size;
    size += base0 % PGSIZE;
    base -= base0 % PGSIZE;
    if (size % PGSIZE != 0)
      size += PGSIZE - size % PGSIZE;

    if (base + size < base){
      printf("modified 7 : base+size<base\n");
      assert(0);
    }

    if (size != size0) {
      fprintf(stderr, "Warning: the memory at  [0x%llX, 0x%llX] has been realigned\n"
                      "to the %ld KiB page size: [0x%llX, 0x%llX]\n",
              base0, base0 + size0 - 1, PGSIZE / 1024, base, base + size - 1);
    }

    res.push_back(std::make_pair(reg_t(base), new mem_t(size)));
    if (!*p)
      break;
    if (*p != ',') {
      puts("modified 7: expected ,")
      assert(0);
    }
    arg = p + 1;
  }

// recognize it if path ends with bbl0
bool htif_helper_bbl0_recognizer(std::string &path) {
  #ifdef VF_DEBUG
    printf("bbl0 recognizer %s\n",path.c_str());
  #endif
  size_t len = path.size();
  if(len-4<0) 
    return 0;
  return path[len-4]=='b' && path[len-3]=='b' && path[len-2]=='l' && path[len-1] == '0';
}

/* Alignment guide for emulator.cc options:
  -x, --long-option        Description with max 80 characters --------------->\n\
       +plus-arg-equivalent\n\
 */
#define HTIF_USAGE_OPTIONS \
"HOST OPTIONS\n\
  -h, --help               Display this help and exit\n\
  +h,  +help\n\
       +permissive         The host will ignore any unparsed options up until\n\
                             +permissive-off (Only needed for VCS)\n\
       +permissive-off     Stop ignoring options. This is mandatory if using\n\
                             +permissive (Only needed for VCS)\n\
      --rfb=DISPLAY        Add new remote frame buffer on display DISPLAY\n\
       +rfb=DISPLAY          to be accessible on 5900 + DISPLAY (default = 0)\n\
      --signature=FILE     Write torture test signature to FILE\n\
       +signature=FILE\n\
      --chroot=PATH        Use PATH as location of syscall-servicing binaries\n\
       +chroot=PATH\n\
      --payload=PATH       Load PATH memory as an additional ELF payload\n\
       +payload=PATH\n\
\n\
HOST OPTIONS (currently unsupported)\n\
      --disk=DISK          Add DISK device. Use a ramdisk since this isn't\n\
       +disk=DISK            supported\n\
\n\
TARGET (RISC-V BINARY) OPTIONS\n\
  These are the options passed to the program executing on the emulated RISC-V\n\
  microprocessor.\n"

#define HTIF_LONG_OPTIONS_OPTIND 1024
#define HTIF_LONG_OPTIONS                                               \
{"help",      no_argument,       0, 'h'                          },     \
{"rfb",       optional_argument, 0, HTIF_LONG_OPTIONS_OPTIND     },     \
{"disk",      required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 1 },     \
{"signature", required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 2 },     \
{"chroot",    required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 3 },     \
{"payload",   required_argument, 0, HTIF_LONG_OPTIONS_OPTIND + 4 },     \
{0, 0, 0, 0}

#define PLUS_PLUS_SEMANTIC 2048
const std::unordered_map<std::string,bool> is_keyword = {
  {"load_pk",1},
  {"load_rom",1},
  {"chrome_rom",1},
  {"load_files",1},
  {"chip_config",1},
  {"shift_file",1},
  {"mems",1},
  {"normal_load",1},
  {"load_to_flash",1}
};

#endif // __HTIF_H
