#include "virt/vmx.h"
#include "console.h"
#include "pmm.h"
#include "string.h"
#include "gdt.h"

extern void vmx_exit_handler(void);
extern void vmx_launch_asm(void);

 
static inline void __vmxon(uint64_t phys_addr) {
    uint8_t ret;
    __asm__ volatile (
        "vmxon %1; setna %0"
        : "=r"(ret)
        : "m"(phys_addr)
        : "cc", "memory"
    );
    if (ret) kprint_str("VMXON Failed\n");
}

static inline void __attribute__((unused)) __vmoff(void) {
    __asm__ volatile ("vmxoff" : : : "cc", "memory");
}

static inline void __vmclear(uint64_t phys_addr) {
    uint8_t ret;
    __asm__ volatile (
        "vmclear %1; setna %0"
        : "=r"(ret)
        : "m"(phys_addr)
        : "cc", "memory"
    );
    if (ret) kprint_str("VMCLEAR Failed\n");
}

static inline void __vmptrld(uint64_t phys_addr) {
    uint8_t ret;
    __asm__ volatile (
        "vmptrld %1; setna %0"
        : "=r"(ret)
        : "m"(phys_addr)
        : "cc", "memory"
    );
    if (ret) kprint_str("VMPTRLD Failed\n");
}

static inline void __vmwrite(uint64_t field, uint64_t value) {
    uint8_t ret;
    __asm__ volatile (
        "vmwrite %1, %2; setna %0"
        : "=r"(ret)
        : "r"(value), "r"(field)
        : "cc"
    );
    if (ret) {
        kprint_str("VMWRITE Failed Field: ");
        kprint_hex(field);
        kprint_newline();
    }
}

static inline uint64_t __vmread(uint64_t field) {
    uint64_t value;
    uint8_t ret;
    __asm__ volatile (
        "vmread %2, %0; setna %1"
        : "=r"(value), "=r"(ret)
        : "r"(field)
        : "cc"
    );
    if (ret) kprint_str("VMREAD Failed\n");
    return value;
}

static inline void __attribute__((unused)) __vmlaunch(void) {
    __asm__ volatile ("vmlaunch" : : : "cc", "memory");
    kprint_str("VMLAUNCH Failed (Should not return)\n");
}

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void write_cr4(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(val));
}

static inline uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr0(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void sgdt(struct gdt_descriptor *gdt) {
    __asm__ volatile ("sgdt %0" : "=m"(*gdt));
}

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static inline void sidt(struct idt_ptr *idtr) {
    __asm__ volatile ("sidt %0" : "=m"(*idtr));
}

static void *vmxon_region;
static uint64_t vmxon_phys;

int vmx_init(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" 
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) 
        : "a"(1));
    
    if (!(ecx & (1 << 5))) {
        kprint_str("VMX: Not supported by CPU\n");
        return -1;
    }
    
    kprint_str("VMX: Supported\n");

    uint64_t cr4 = read_cr4();
    write_cr4(cr4 | (1 << 13));  

    uint64_t vmx_basic = read_msr(MSR_IA32_VMX_BASIC);
    uint32_t revision_id = (uint32_t)vmx_basic;
    
    vmxon_region = pmm_alloc_page();  
    if (!vmxon_region) return -1;
    
    vmxon_phys = (uint64_t)vmxon_region; 
    
    *(uint32_t*)vmxon_region = revision_id;
    
    __vmxon(vmxon_phys);
    
    kprint_str("VMX: Enabled successfully\n");
    return 0;
}

static void vmx_setup_host_state() {
    __vmwrite(HOST_CR0, read_cr0());
    __vmwrite(HOST_CR3, read_cr3());
    __vmwrite(HOST_CR4, read_cr4());

    __vmwrite(HOST_CS_SELECTOR, 0x08);  
    __vmwrite(HOST_SS_SELECTOR, 0x10);  
    __vmwrite(HOST_DS_SELECTOR, 0x10);
    __vmwrite(HOST_ES_SELECTOR, 0x10);
    __vmwrite(HOST_FS_SELECTOR, 0x10);
    __vmwrite(HOST_GS_SELECTOR, 0x10);
    __vmwrite(HOST_TR_SELECTOR, 0x28);  
    
    struct gdt_descriptor gdtr;
    sgdt(&gdtr);
    __vmwrite(HOST_GDTR_BASE, gdtr.offset);
    
    struct idt_ptr idtr;
    sidt(&idtr);
    __vmwrite(HOST_IDTR_BASE, idtr.base);

    struct gdt_entry *gdt = (struct gdt_entry *)gdtr.offset;
    struct gdt_entry *tss_entry = &gdt[5];  
    
    uint64_t tss_base = 0;
    tss_base |= (uint64_t)tss_entry->base_low;
    tss_base |= (uint64_t)tss_entry->base_middle << 16;
    tss_base |= (uint64_t)tss_entry->base_high << 24;

    struct gdt_entry *upper_entry = &gdt[6];

    uint32_t base_upper = *(uint32_t*)upper_entry;
    tss_base |= (uint64_t)base_upper << 32;

    __vmwrite(HOST_TR_BASE, tss_base);
    __vmwrite(HOST_RIP, (uint64_t)vmx_exit_handler);
}

static uint32_t vmx_adjust_controls(uint32_t ctl, uint32_t msr) {
    uint64_t msr_val = read_msr(msr);
    uint32_t allowed_0 = (uint32_t)msr_val;
    uint32_t allowed_1 = (uint32_t)(msr_val >> 32);
    
    ctl &= allowed_1;
    ctl |= allowed_0;
    return ctl;
}

static void vmx_setup_guest_state() {
     
    uint64_t cr0 = 0x60000010;  
     
    __vmwrite(GUEST_CR0, cr0);
    __vmwrite(CR0_READ_SHADOW, cr0);
    __vmwrite(GUEST_CR3, 0);

    __vmwrite(GUEST_CR4, 0);
    __vmwrite(CR4_READ_SHADOW, 0);

    __vmwrite(GUEST_CS_SELECTOR, 0);
    __vmwrite(GUEST_CS_BASE, 0);
    __vmwrite(GUEST_CS_LIMIT, 0xFFFF);
    __vmwrite(GUEST_CS_AR_BYTES, 0x9B);

    __vmwrite(GUEST_DS_SELECTOR, 0);
    __vmwrite(GUEST_DS_BASE, 0);
    __vmwrite(GUEST_DS_LIMIT, 0xFFFF);
    __vmwrite(GUEST_DS_AR_BYTES, 0x93);

    __vmwrite(GUEST_ES_SELECTOR, 0);
    __vmwrite(GUEST_ES_BASE, 0);
    __vmwrite(GUEST_ES_LIMIT, 0xFFFF);
    __vmwrite(GUEST_ES_AR_BYTES, 0x93);

    __vmwrite(GUEST_SS_SELECTOR, 0);
    __vmwrite(GUEST_SS_BASE, 0);
    __vmwrite(GUEST_SS_LIMIT, 0xFFFF);
    __vmwrite(GUEST_SS_AR_BYTES, 0x93);

    __vmwrite(GUEST_FS_SELECTOR, 0);
    __vmwrite(GUEST_FS_BASE, 0);
    __vmwrite(GUEST_FS_LIMIT, 0xFFFF);
    __vmwrite(GUEST_FS_AR_BYTES, 0x93);

    __vmwrite(GUEST_GS_SELECTOR, 0);
    __vmwrite(GUEST_GS_BASE, 0);
    __vmwrite(GUEST_GS_LIMIT, 0xFFFF);
    __vmwrite(GUEST_GS_AR_BYTES, 0x93);

    __vmwrite(GUEST_LDTR_SELECTOR, 0);
    __vmwrite(GUEST_LDTR_BASE, 0);
    __vmwrite(GUEST_LDTR_LIMIT, 0);
    __vmwrite(GUEST_LDTR_AR_BYTES, 0x10000);  

    __vmwrite(GUEST_TR_SELECTOR, 0);
    __vmwrite(GUEST_TR_BASE, 0);
    __vmwrite(GUEST_TR_LIMIT, 0xFFFF);
    __vmwrite(GUEST_TR_AR_BYTES, 0x8B);

    __vmwrite(GUEST_GDTR_BASE, 0);
    __vmwrite(GUEST_GDTR_LIMIT, 0xFFFF);
    __vmwrite(GUEST_IDTR_BASE, 0);
    __vmwrite(GUEST_IDTR_LIMIT, 0xFFFF);

    __vmwrite(GUEST_RIP, 0x7C00);
    __vmwrite(GUEST_RFLAGS, 0x02);  
    __vmwrite(GUEST_RSP, 0x0000);

    __vmwrite(GUEST_DR7, 0x00000400);

    __vmwrite(GUEST_SYSENTER_CS, 0);
    __vmwrite(GUEST_SYSENTER_ESP, 0);
    __vmwrite(GUEST_SYSENTER_EIP, 0);
    
    __vmwrite(GUEST_IA32_EFER, 0);
    __vmwrite(GUEST_IA32_PAT, 0x0007040600070406ULL);  
    __vmwrite(GUEST_IA32_DEBUGCTL, 0);

    __vmwrite(VMCS_LINK_POINTER, 0xFFFFFFFF);
    __vmwrite(VMCS_LINK_POINTER_HIGH, 0xFFFFFFFF);
}

static void vmx_setup_controls() {

    uint32_t pin_ctls = vmx_adjust_controls(0, MSR_IA32_VMX_PINBASED_CTLS);
    __vmwrite(PIN_BASED_VM_EXEC_CONTROL, pin_ctls);

    uint32_t proc_ctls = (1 << 7) | (1 << 31); 
    proc_ctls = vmx_adjust_controls(proc_ctls, MSR_IA32_VMX_PROCBASED_CTLS);
    __vmwrite(CPU_BASED_VM_EXEC_CONTROL, proc_ctls);

    uint32_t secondary = (1 << 1) | (1 << 7);
    __vmwrite(SECONDARY_VM_EXEC_CONTROL, secondary);

    uint32_t exit_ctls = (1 << 9); 
    exit_ctls = vmx_adjust_controls(exit_ctls, MSR_IA32_VMX_EXIT_CTLS);
    __vmwrite(VM_EXIT_CONTROLS, exit_ctls);

    uint32_t entry_ctls = 0;
    entry_ctls = vmx_adjust_controls(entry_ctls, MSR_IA32_VMX_ENTRY_CTLS);
    __vmwrite(VM_ENTRY_CONTROLS, entry_ctls); 
}

static void vmx_setup_ept() {

    uint64_t *pml4 = (uint64_t*)pmm_alloc_page();
    uint64_t *pdpt = (uint64_t*)pmm_alloc_page();
    uint64_t *pd   = (uint64_t*)pmm_alloc_page();

    memset(pml4, 0, 4096);
    memset(pdpt, 0, 4096);
    memset(pd, 0, 4096);
    
    pml4[0] = (uint64_t)pdpt | 0x7;  
    pdpt[0] = (uint64_t)pd | 0x7;

    for (int i = 0; i < 512; i++) {
        pd[i] = (uint64_t)(i * 0x200000) | 0x37;  
        pd[i] = (uint64_t)(i * 0x200000) | 0xB7;
    }

    uint64_t eptp = (uint64_t)pml4 | (6 | (3 << 3));
    __vmwrite(EPT_POINTER, eptp);
    __vmwrite(EPT_POINTER_HIGH, eptp >> 32);
}

int vmx_create_vm(void) {
    void *vmcs_region = pmm_alloc_page();
    if (!vmcs_region) return -1;
    
    uint64_t vmx_basic = read_msr(MSR_IA32_VMX_BASIC);
    *(uint32_t*)vmcs_region = (uint32_t)vmx_basic;
    
    uint64_t phys = (uint64_t)vmcs_region;
    
    __vmclear(phys);
    __vmptrld(phys);
    
    kprint_str("VMX: VMCS Loaded and Cleared\n");
    
    vmx_setup_host_state();
    vmx_setup_guest_state();
    vmx_setup_ept();
    vmx_setup_controls();
    
    kprint_str("VMX: VM Configured\n");
    
    return 0;
}

void vmx_handle_exit(struct guest_regs *regs) {
    uint64_t exit_reason = __vmread(VM_EXIT_REASON);
    
    if (exit_reason & 0x80000000) {
        kprint_str("VM Entry Failure! Reason: ");
        kprint_hex(exit_reason & 0xFFFF);
        kprint_newline();
        while(1);
    }

    uint64_t guest_rip = __vmread(GUEST_RIP);
    uint64_t guest_len = __vmread(VM_EXIT_INSTRUCTION_LEN);

    if ((exit_reason & 0xFFFF) == 10) {  
        uint32_t eax, ebx, ecx, edx;
        __asm__ volatile ("cpuid" 
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) 
            : "a"((uint32_t)regs->rax), "c"((uint32_t)regs->rcx));
        
        regs->rax = eax;
        regs->rbx = ebx;
        regs->rcx = ecx;
        regs->rdx = edx;
        
        __vmwrite(GUEST_RIP, guest_rip + guest_len);
    } else if ((exit_reason & 0xFFFF) == 12) {  
        kprint_str("VM Halted.\n");
        __vmwrite(GUEST_RIP, guest_rip + guest_len);
         
    } else {
        kprint_str("Unhandled VM Exit, Reason: ");
        kprint_hex(exit_reason);
        kprint_str(" RIP: ");
        kprint_hex(guest_rip);
        kprint_newline();
        while(1);
    }
}

void vmx_launch(void) {
    uint64_t rsp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
    __vmwrite(HOST_RSP, rsp);

    kprint_str("Launching VM...\n");
    vmx_launch_asm();
    kprint_str("VMLAUNCH Failed (Returned from asm)\n");
}
