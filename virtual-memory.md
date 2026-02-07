Virtual Memory: The Full Picture
The Problem
Without virtual memory, every program sees raw physical RAM. If your machine has 128 MB of RAM, addresses go from 0x00000000 to 0x07FFFFFF and that's it. Every program shares that space. One buggy program can stomp on another's memory or crash the kernel.

Virtual memory gives each process its own private view of memory. The CPU translates every memory access through a mapping layer before it hits physical RAM:


Program thinks:  "I'm writing to address 0x00400000"
                          │
                    ┌─────▼──────┐
                    │  MMU (CPU)  │  ← hardware translation
                    └─────┬──────┘
                          │
Physical RAM:     "Actually goes to 0x037A2000"
The Two-Level Page Table (i686 / 32-bit x86)
On 32-bit x86, a virtual address is 32 bits → can address 4 GB. Mapping every single byte would be insane, so the CPU works in 4 KB pages (4096 bytes). That's 4 GB / 4 KB = 1,048,576 pages to track.

A flat table of 1M entries × 4 bytes = 4 MB per process. That's wasteful because most of those entries would be empty. So x86 uses a two-level structure to be sparse:


          32-bit Virtual Address
┌──────────┬──────────┬──────────────┐
│ Bits 31-22│ Bits 21-12│ Bits 11-0   │
│ PD Index  │ PT Index  │ Page Offset │
│ (10 bits) │ (10 bits) │ (12 bits)   │
└─────┬─────┴─────┬─────┴──────┬──────┘
      │           │            │
      │           │            └─► Byte within the 4 KB page (0-4095)
      │           └──────────────► Which entry in the Page Table (0-1023)
      └──────────────────────────► Which entry in the Page Directory (0-1023)
This is exactly what the code does in paging.c:63-66:


uint32_t pd_index = virtual_addr >> 22;            // top 10 bits
uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;  // middle 10 bits
// the bottom 12 bits are the offset within the page
Page Directory (Level 1)
The Page Directory is a single 4 KB table with 1024 entries. It lives at the physical address stored in the CPU's CR3 register. Each entry either points to a Page Table or is marked "not present."


CR3 Register ──────►  PAGE DIRECTORY (4 KB, 1024 entries)
                     ┌─────────────────────────────────────────┐
              [0]    │ Page Table 0 addr  | flags (P,R/W,U/S)  │ → covers 0x00000000 - 0x003FFFFF (4 MB)
              [1]    │ Page Table 1 addr  | flags               │ → covers 0x00400000 - 0x007FFFFF (4 MB)
              [2]    │ Page Table 2 addr  | flags               │ → covers 0x00800000 - 0x00BFFFFF (4 MB)
              [3]    │ Page Table 3 addr  | flags               │ → covers 0x00C00000 - 0x00FFFFFF (4 MB)
              [4]    │ 0x00000000         | NOT PRESENT         │ → nothing mapped here
              ...    │ ...                                      │
              [1023] │ 0x00000000         | NOT PRESENT         │ → covers 0xFFC00000 - 0xFFFFFFFF
                     └─────────────────────────────────────────┘
Each entry covers a 4 MB chunk of virtual space (1024 pages × 4 KB = 4 MB).

The code creates this at paging.c:7:


static page_dir_entry_t page_directory[PAGE_ENTRIES] __attribute__((aligned(4096)));
And initializes entries 0-3 to cover the first 16 MB at paging.c:46:


page_directory[t] = ((uint32_t)page_tables[t]) | PAGE_PRESENT | PAGE_WRITE;
Page Tables (Level 2)
Each Page Table is also a 4 KB table with 1024 entries. Each entry maps one 4 KB virtual page to one 4 KB physical frame.


PAGE DIRECTORY [0] ──►  PAGE TABLE 0 (4 KB, 1024 entries)
                       ┌───────────────────────────────────────┐
                [0]    │ Phys frame 0x00000 | P, R/W            │ → virt 0x00000000 maps to phys 0x00000000
                [1]    │ Phys frame 0x01000 | P, R/W            │ → virt 0x00001000 maps to phys 0x00001000
                [2]    │ Phys frame 0x02000 | P, R/W            │ → virt 0x00002000 maps to phys 0x00002000
                ...    │ ...                                    │
                [1023] │ Phys frame 0x3FF000 | P, R/W           │ → virt 0x003FF000 maps to phys 0x003FF000
                       └───────────────────────────────────────┘
Your init loop at paging.c:41-44 fills these in as identity mappings (virtual == physical):


for (int t = 0; t < 4; t++) {
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        uint32_t addr = (t * PAGE_ENTRIES + i) * PAGE_SIZE;
        page_tables[t][i] = addr | PAGE_PRESENT | PAGE_WRITE;
    }
}
Full Address Translation Walkthrough
Let's trace a real example. Say code reads from virtual address 0x00B01234:


Virtual address: 0x00B01234
Binary: 0000 0000 1011 0000 0001 0010 0011 0100

Step 1 - Split the address:
┌──────────────┬──────────────┬──────────────────┐
│  PD Index    │  PT Index    │  Offset          │
│  0000000010  │  1100000001  │  001000110100     │
│  = 2         │  = 769       │  = 0x234          │
└──────┬───────┴──────┬───────┴────────┬─────────┘
       │              │                │
       ▼              │                │
Step 2 - Read page_directory[2]        │
       │              │                │
       │  It says: "Page Table at      │
       │   physical addr 0x00XXX000"   │
       │              │                │
       │              ▼                │
       │   Step 3 - Read that          │
       │   page_table[769]             │
       │              │                │
       │              │  It says:      │
       │              │  "Physical     │
       │              │   frame at     │
       │              │   0x00B01000"  │
       │              │                │
       │              ▼                ▼
       │   Step 4 - Combine: frame + offset
       │        0x00B01000 + 0x234 = 0x00B01234
       │
       └──► (identity mapped, so virt == phys in this case)
This all happens in hardware — the CPU's MMU does it automatically on every memory access. No software involved (unless a page fault occurs).

Entry Format (32 bits)
Each Page Directory Entry and Page Table Entry is a 32-bit integer packed like this:


Page Directory Entry:
┌────────────────────────────┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
│  Page Table Address (20)   │G│S│0│A│D│W│U│R│P│ │ │ │
│  Bits 31-12                │ │ │ │ │ │ │/│/│ │ │ │ │
│  (phys addr, 4K aligned)   │ │ │ │ │ │ │S│W│ │ │ │ │
└────────────────────────────┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
 Bit 0  (P)   = Present        ← your PAGE_PRESENT  (0x001)
 Bit 1  (R/W) = Read/Write     ← your PAGE_WRITE    (0x002)
 Bit 2  (U/S) = User/Supervisor← your PAGE_USER     (0x004)
 Bit 5  (A)   = Accessed       ← your PAGE_ACCESSED (0x020)
 Bit 6  (D)   = Dirty          ← your PAGE_DIRTY    (0x040)

The top 20 bits = physical address of the page table (or frame)
The bottom 12 bits = flags
That's why our code uses & 0xFFFFF000 to extract the address and & 0xFFF for the flags at paging.c:87:


page_table_entry_t* table = (page_table_entry_t*)(page_directory[pd_index] & 0xFFFFF000);
Why Two Levels? (The Sparseness Win)

FLAT TABLE approach (what we DON'T do):
┌─────────────────────────────────────────────┐
│ 1,048,576 entries × 4 bytes = 4 MB per process │  ← wasteful!
│ Most entries are empty/not-present              │
└─────────────────────────────────────────────┘

TWO-LEVEL approach (what x86 does):
┌──────────────────────────────────────────────────────┐
│ 1 Page Directory                    =  4 KB          │
│ + only the Page Tables you need     = ~4 KB each     │
│                                                      │
│ Typical process uses maybe 5-10 page tables          │
│ Total: 4 KB + (10 × 4 KB) = ~44 KB    ← much better │
└──────────────────────────────────────────────────────┘
A "not present" entry in the Page Directory means that entire 4 MB region doesn't need a page table allocated at all. For a typical process that only uses a few MB of memory, you save enormous amounts of space.

Where Your VMM Fits In
Uur two layers build on each other like this:


┌─────────────────────────────────────────────────────────┐
│                    VMM (vmm.c/vmm.h)                    │  ← POLICY
│                                                         │
│  "Track WHAT is mapped and WHY"                         │
│  • Address spaces (one per process, later)              │
│  • Regions (code, data, heap, stack, MMIO)              │
│  • Permission tracking (read/write/exec/user)           │
│  • Finding free virtual address ranges                  │
│  • Page fault handling (future demand paging)           │
├─────────────────────────────────────────────────────────┤
│                  Paging (paging.c/paging.h)              │  ← MECHANISM
│                                                         │
│  "Map/unmap individual pages in hardware"               │
│  • Manipulate page directory + page table entries        │
│  • Load CR3, enable paging, flush TLB                   │
│  • Raw map: virt_addr → phys_addr                       │
├─────────────────────────────────────────────────────────┤
│                  PMM (pmm.c/pmm.h)                      │  ← PHYSICAL
│                                                         │
│  "Hand out physical 4 KB frames"                        │
│  • Bitmap of free/used physical frames                  │
│  • pmm_alloc_frame() / pmm_free_frame()                 │
└─────────────────────────────────────────────────────────┘
When your VMM wants to map a region, the flow is:


vmm_map_region(space, 0xC0000000, 0x1000, VMM_READ|VMM_WRITE, REGION_KERNEL_HEAP)
    │
    ├─► 1. Allocate a region struct from region_pool
    ├─► 2. Record: "vaddr 0xC0000000, size 4KB, writable, heap"
    ├─► 3. For each page in the region:
    │       ├─► pmm_alloc_frame()        → get physical frame (e.g. 0x037A2000)
    │       └─► paging_map_page(0xC0000000, 0x037A2000, PAGE_PRESENT|PAGE_WRITE)
    │               │
    │               ├─► pd_index = 0xC0000000 >> 22 = 768
    │               ├─► pt_index = (0xC0000000 >> 12) & 0x3FF = 0
    │               ├─► If page_directory[768] not present → pmm_alloc_frame() for new page table
    │               └─► page_table[0] = 0x037A2000 | PRESENT | WRITE
    │
    └─► 4. Insert region into sorted linked list
Address Spaces & Context Switching (The Future)
Right now we have one address space (kernel_space). When you add processes:


Process A's view:                    Process B's view:
┌──────────────────┐                 ┌──────────────────┐
│ 0xC0000000+      │ ← Kernel        │ 0xC0000000+      │ ← SAME kernel
│ (shared, same    │   mappings      │ (shared, same    │   mappings
│  page tables)    │                 │  page tables)    │
├──────────────────┤                 ├──────────────────┤
│ 0x08048000       │ ← A's code      │ 0x08048000       │ ← B's code
│ 0x08060000       │ ← A's heap      │ 0x08060000       │ ← B's heap
│ 0xBFFFF000       │ ← A's stack     │ 0xBFFFF000       │ ← B's stack
└──────────────────┘                 └──────────────────┘
 CR3 → A's page dir                  CR3 → B's page dir

Context switch: just load B's page dir into CR3
    mov cr3, [B's_page_directory_physical_addr]
Same virtual addresses, different physical frames. Process A's 0x08048000 might map to physical 0x02000000, while Process B's 0x08048000 maps to physical 0x05000000. Total isolation — neither can see the other's memory.

Your vmm_switch_address_space() at vmm.h:76 is what does this — it just swaps CR3.

TL;DR
Concept	What it is	Size	Your code
Page	4 KB chunk of virtual memory	4096 bytes	PAGE_SIZE
Frame	4 KB chunk of physical RAM	4096 bytes	pmm_alloc_frame()
Page Table	1024 entries, each maps 1 page → 1 frame	4 KB	page_table_entry_t[1024]
Page Directory	1024 entries, each points to a page table	4 KB	page_dir_entry_t[1024]
CR3	CPU register holding physical addr of page dir	32 bits	paging_load_directory()
Address Space	Page directory + all its regions	varies	vmm_address_space_t
Region	Contiguous virtual range with same perms/purpose	varies	vmm_region_t
The CPU does the two-level lookup automatically on every memory access. Paging gives you the raw map/unmap tools. The VMM tracks the meaning of those mappings so your OS can manage memory intelligently.