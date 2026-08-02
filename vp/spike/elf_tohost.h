// elf_tohost.h - extracts the tohost symbol address from an ELF (standalone, independent of the spike version)
//   Parses the ELF32 symbol table directly, without depending on spike's sim_t.
//   The riscv-tests convention: the exit code is written to the tohost symbol.
#ifndef ELF_TOHOST_H
#define ELF_TOHOST_H
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// Minimal ELF32 structures (for RV32)
struct Elf32_Ehdr_min { unsigned char e_ident[16]; uint16_t e_type, e_machine;
  uint32_t e_version, e_entry, e_phoff, e_shoff, e_flags;
  uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx; };
struct Elf32_Shdr_min { uint32_t sh_name, sh_type, sh_flags, sh_addr, sh_offset,
  sh_size, sh_link, sh_info, sh_addralign, sh_entsize; };
struct Elf32_Sym_min { uint32_t st_name, st_value, st_size;
  unsigned char st_info, st_other; uint16_t st_shndx; };

// Returns the tohost symbol address (0 if not found)
static inline uint64_t elf_find_tohost(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return 0;
  Elf32_Ehdr_min eh;
  if (fread(&eh, 1, sizeof(eh), f) != sizeof(eh)) { fclose(f); return 0; }
  if (memcmp(eh.e_ident, "\x7f""ELF", 4) != 0) { fclose(f); return 0; }

  // Read the section headers
  uint64_t result = 0;
  for (int i = 0; i < eh.e_shnum; i++) {
    Elf32_Shdr_min sh;
    fseek(f, eh.e_shoff + (long)i * eh.e_shentsize, SEEK_SET);
    if (fread(&sh, 1, sizeof(sh), f) != sizeof(sh)) continue;
    if (sh.sh_type != 2) continue;                 // SHT_SYMTAB
    // The linked string table
    Elf32_Shdr_min strsh;
    fseek(f, eh.e_shoff + (long)sh.sh_link * eh.e_shentsize, SEEK_SET);
    if (fread(&strsh, 1, sizeof(strsh), f) != sizeof(strsh)) continue;
    // Iterate the symbols
    int nsym = sh.sh_entsize ? (sh.sh_size / sh.sh_entsize) : 0;
    for (int s = 0; s < nsym; s++) {
      Elf32_Sym_min sym;
      fseek(f, sh.sh_offset + (long)s * sh.sh_entsize, SEEK_SET);
      if (fread(&sym, 1, sizeof(sym), f) != sizeof(sym)) continue;
      char name[64] = {0};
      fseek(f, strsh.sh_offset + sym.st_name, SEEK_SET);
      if (!fgets(name, sizeof(name), f)) continue;
      if (strncmp(name, "tohost", 6) == 0 && (name[6]=='\0')) {
        result = sym.st_value; break;
      }
    }
    if (result) break;
  }
  fclose(f);
  return result;
}
#endif // ELF_TOHOST_H
