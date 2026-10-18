#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>

// ============================================================================
// TakePeek 2.0 WD — Kernel Driver (kdmapper compatible)
// Uses IoCreateDriver + device IOCTL for usermode communication
// Build: requires WDK — see build_driver.bat
// ============================================================================

#define DEVICE_NAME  L"\\Device\\TpkWd0"
#define SYMLINK_NAME L"\\DosDevices\\TpkWd0"
#define IOCTL_PROCESS_REQUEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define REQUEST_MAGIC 0x44524B4E

typedef enum _DRIVER_CMD {
    CMD_NONE         = 0,
    CMD_READ         = 1,
    CMD_WRITE        = 2,
    CMD_MODULE_BASE  = 3,
    CMD_ALLOC        = 4,
    CMD_FREE         = 5,
    CMD_PROTECT      = 6,
    CMD_READ64       = 7,
    CMD_WRITE64      = 8,
    CMD_GET_PEB      = 10,
    CMD_PING         = 99,
} DRIVER_CMD;

#pragma pack(push, 8)
typedef struct _REQUEST_DATA {
    ULONG       magic;
    ULONG       command;
    ULONG64     pid;
    ULONG64     address;
    ULONG64     buffer;
    ULONG64     size;
    ULONG64     result;
    ULONG       protect;
    WCHAR       module_name[64];
} REQUEST_DATA, *PREQUEST_DATA;
#pragma pack(pop)

// -- Undocumented kernel APIs ---------------------------------------------

NTKERNELAPI NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS   SourceProcess,
    PVOID       SourceAddress,
    PEPROCESS   TargetProcess,
    PVOID       TargetAddress,
    SIZE_T      BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T     ReturnSize);

NTKERNELAPI PVOID NTAPI PsGetProcessPeb(PEPROCESS Process);

NTKERNELAPI NTSTATUS NTAPI ZwProtectVirtualMemory(
    HANDLE ProcessHandle, PVOID* BaseAddress,
    PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect);

NTKERNELAPI NTSTATUS IoCreateDriver(
    PUNICODE_STRING DriverName,
    PDRIVER_INITIALIZE InitializationFunction);

// -- PEB / LDR structures for module walking ------------------------------

typedef struct _PEB_LDR_DATA_K {
    ULONG       Length;
    UCHAR       Initialized;
    PVOID       SsHandle;
    LIST_ENTRY  InLoadOrderModuleList;
    LIST_ENTRY  InMemoryOrderModuleList;
} PEB_LDR_DATA_K;

typedef struct _LDR_DATA_TABLE_ENTRY_K {
    LIST_ENTRY     InLoadOrderLinks;
    LIST_ENTRY     InMemoryOrderLinks;
    LIST_ENTRY     InInitializationOrderLinks;
    PVOID          DllBase;
    PVOID          EntryPoint;
    ULONG          SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY_K;

typedef struct _PEB_K {
    UCHAR          Reserved1[2];
    UCHAR          BeingDebugged;
    UCHAR          Reserved2[1];
    PVOID          Reserved3[2];
    PEB_LDR_DATA_K* Ldr;
} PEB_K;

// -- Globals --------------------------------------------------------------

static PDEVICE_OBJECT g_deviceObj = NULL;

// -- Memory operations ----------------------------------------------------

static NTSTATUS KReadMemory(ULONG64 pid, ULONG64 srcAddr, ULONG64 dstBuf, ULONG64 sz) {
    if (!srcAddr || !dstBuf || !sz || sz > 0x10000000)
        return STATUS_INVALID_PARAMETER;
    if (srcAddr >= 0x7FFFFFFFFFFF)
        return STATUS_ACCESS_VIOLATION;

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)pid, &process);
    if (!NT_SUCCESS(status)) return status;

    SIZE_T bytes = 0;
    __try {
        status = MmCopyVirtualMemory(
            process, (PVOID)srcAddr,
            PsGetCurrentProcess(), (PVOID)dstBuf,
            (SIZE_T)sz, KernelMode, &bytes);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }

    ObDereferenceObject(process);
    return status;
}

static NTSTATUS KWriteMemory(ULONG64 pid, ULONG64 dstAddr, ULONG64 srcBuf, ULONG64 sz) {
    if (!dstAddr || !srcBuf || !sz || sz > 0x10000000)
        return STATUS_INVALID_PARAMETER;
    if (dstAddr >= 0x7FFFFFFFFFFF)
        return STATUS_ACCESS_VIOLATION;

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)pid, &process);
    if (!NT_SUCCESS(status)) return status;

    SIZE_T bytes = 0;
    __try {
        status = MmCopyVirtualMemory(
            PsGetCurrentProcess(), (PVOID)srcBuf,
            process, (PVOID)dstAddr,
            (SIZE_T)sz, KernelMode, &bytes);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }

    ObDereferenceObject(process);
    return status;
}

// -- Module base via PEB walk ---------------------------------------------

static ULONG64 KGetModuleBase(ULONG64 pid, const WCHAR* moduleName) {
    PEPROCESS process = NULL;
    if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)pid, &process)))
        return 0;

    PEB_K* peb = (PEB_K*)PsGetProcessPeb(process);
    if (!peb) { ObDereferenceObject(process); return 0; }

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);

    ULONG64 result = 0;

    __try {
        PEB_LDR_DATA_K* ldr = peb->Ldr;
        if (!ldr) __leave;

        LIST_ENTRY* head = &ldr->InLoadOrderModuleList;
        LIST_ENTRY* entry = head->Flink;

        while (entry != head) {
            LDR_DATA_TABLE_ENTRY_K* mod =
                CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY_K, InLoadOrderLinks);

            if (mod->BaseDllName.Buffer && mod->BaseDllName.Length > 0) {
                UNICODE_STRING target;
                RtlInitUnicodeString(&target, moduleName);
                if (RtlCompareUnicodeString(&mod->BaseDllName, &target, TRUE) == 0) {
                    result = (ULONG64)mod->DllBase;
                    break;
                }
            }
            entry = entry->Flink;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        result = 0;
    }

    if (result == 0) {
        __try {
            result = *(ULONG64*)((UCHAR*)peb + 0x10);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            result = 0;
        }
    }

    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);
    return result;
}

// -- PEB address ----------------------------------------------------------

static ULONG64 KGetPeb(ULONG64 pid) {
    PEPROCESS process = NULL;
    if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)pid, &process)))
        return 0;
    ULONG64 peb = (ULONG64)PsGetProcessPeb(process);
    ObDereferenceObject(process);
    return peb;
}

// -- Virtual memory operations --------------------------------------------

static ULONG64 KAllocMemory(ULONG64 pid, ULONG64 sz, ULONG protect) {
    PEPROCESS process = NULL;
    if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)pid, &process)))
        return 0;

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);

    PVOID base = NULL;
    SIZE_T regionSize = (SIZE_T)sz;
    NTSTATUS status = ZwAllocateVirtualMemory(
        ZwCurrentProcess(), &base, 0, &regionSize,
        MEM_COMMIT | MEM_RESERVE, protect);

    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);
    return NT_SUCCESS(status) ? (ULONG64)base : 0;
}

static void KFreeMemory(ULONG64 pid, ULONG64 address) {
    PEPROCESS process = NULL;
    if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)pid, &process)))
        return;

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);

    PVOID base = (PVOID)address;
    SIZE_T regionSize = 0;
    ZwFreeVirtualMemory(ZwCurrentProcess(), &base, &regionSize, MEM_RELEASE);

    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);
}

static void KProtectMemory(ULONG64 pid, ULONG64 address, ULONG64 sz, ULONG protect) {
    PEPROCESS process = NULL;
    if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)pid, &process)))
        return;

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);

    PVOID base = (PVOID)address;
    SIZE_T regionSize = (SIZE_T)sz;
    ULONG oldProtect = 0;
    ZwProtectVirtualMemory(ZwCurrentProcess(), &base, &regionSize, protect, &oldProtect);

    KeUnstackDetachProcess(&apc);
    ObDereferenceObject(process);
}

// -- Request handler ------------------------------------------------------

static void ProcessRequest(PREQUEST_DATA req) {
    req->result = 0;

    switch (req->command) {
    case CMD_PING:
        req->result = 0x4B524E4C; // "KRNL"
        break;

    case CMD_READ:
        req->result = NT_SUCCESS(KReadMemory(
            req->pid, req->address, req->buffer, req->size)) ? 1 : 0;
        break;

    case CMD_WRITE:
        req->result = NT_SUCCESS(KWriteMemory(
            req->pid, req->address, req->buffer, req->size)) ? 1 : 0;
        break;

    case CMD_MODULE_BASE:
        req->result = KGetModuleBase(req->pid, req->module_name);
        break;

    case CMD_GET_PEB:
        req->result = KGetPeb(req->pid);
        break;

    case CMD_ALLOC:
        req->result = KAllocMemory(req->pid, req->size, req->protect);
        break;

    case CMD_FREE:
        KFreeMemory(req->pid, req->result);
        req->result = 1;
        break;

    case CMD_PROTECT:
        KProtectMemory(req->pid, req->address, req->size, req->protect);
        req->result = 1;
        break;

    default:
        break;
    }
}

// -- IRP handlers ---------------------------------------------------------

static NTSTATUS IrpCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS IrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG bytesReturned = 0;

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_PROCESS_REQUEST) {
        ULONG inLen = stack->Parameters.DeviceIoControl.InputBufferLength;
        ULONG outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;

        if (inLen >= sizeof(REQUEST_DATA) && outLen >= sizeof(REQUEST_DATA)) {
            PREQUEST_DATA req = (PREQUEST_DATA)Irp->AssociatedIrp.SystemBuffer;
            if (req->magic == REQUEST_MAGIC) {
                __try {
                    ProcessRequest(req);
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    req->result = 0;
                }
                bytesReturned = sizeof(REQUEST_DATA);
                status = STATUS_SUCCESS;
            }
        }
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// -- Device initialization (called by IoCreateDriver) ---------------------

static NTSTATUS DeviceInit(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING devName, symLink;
    RtlInitUnicodeString(&devName, DEVICE_NAME);
    RtlInitUnicodeString(&symLink, SYMLINK_NAME);

    NTSTATUS status = IoCreateDevice(
        DriverObject, 0, &devName,
        FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
        FALSE, &g_deviceObj);
    if (!NT_SUCCESS(status)) return status;

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_deviceObj);
        g_deviceObj = NULL;
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = IrpCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = IrpCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IrpDeviceControl;
    DriverObject->DriverUnload = NULL;

    g_deviceObj->Flags |= DO_BUFFERED_IO;
    g_deviceObj->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

// -- Driver Entry ---------------------------------------------------------

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING drvName;
    RtlInitUnicodeString(&drvName, L"\\Driver\\TpkWd0");

    return IoCreateDriver(&drvName, DeviceInit);
}
