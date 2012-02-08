/*
 * PROJECT:         ReactOS Universal Serial Bus Hub Driver
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            drivers/usb/usbhub/fdo.c
 * PURPOSE:         UsbHub Driver
 * PROGRAMMERS:
 *                  Herv� Poussineau (hpoussin@reactos.org)
 *                  Michael Martin (michael.martin@reactos.org)
 *                  Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "usbhub.h"

NTSTATUS NTAPI
USBHUB_Create(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    DPRINT1("USBHUB: IRP_MJ_CREATE\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
USBHUB_Close(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    DPRINT1("USBHUB: IRP_MJ_CLOSE\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS NTAPI
USBHUB_Cleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    DPRINT1("USBHUB: IRP_MJ_CLEANUP\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}


NTSTATUS NTAPI
USBHUB_AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject)
{
    PDEVICE_OBJECT DeviceObject;
    PHUB_DEVICE_EXTENSION HubDeviceExtension;
    NTSTATUS Status;
    DPRINT1("USBHUB: AddDevice\n");
    //
    // Create the Device Object
    //
    Status = IoCreateDevice(DriverObject,
                            sizeof(HUB_DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_BUS_EXTENDER,
                            FILE_AUTOGENERATED_DEVICE_NAME,
                            FALSE,
                            &DeviceObject);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBHUB: IoCreateDevice() failed with status 0x%08lx\n", Status);
        return Status;
    }

    //
    // Zero Hub Extension
    //
    HubDeviceExtension = (PHUB_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    RtlZeroMemory(HubDeviceExtension, sizeof(HUB_DEVICE_EXTENSION));

    //
    // Set this to Fdo
    //
    HubDeviceExtension->Common.IsFDO = TRUE;
    DeviceObject->Flags |= DO_POWER_PAGABLE;

    //
    // initialize reset complete event
    //
    KeInitializeEvent(&HubDeviceExtension->ResetComplete, NotificationEvent, FALSE);

    //
    // Attached to lower device
    //
    //Status = IoAttachDeviceToDeviceStackSafe(Fdo, Pdo, &DeviceExtension->LowerDevice);
    HubDeviceExtension->LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBHUB: IoAttachDeviceToDeviceStackSafe() failed with status 0x%08lx\n", Status);
        IoDeleteDevice(DeviceObject);
        return Status;
    }

    DeviceObject->Flags |= DO_BUFFERED_IO;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

static NTSTATUS NTAPI
USBHUB_IrpStub(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    NTSTATUS Status;

    if (((PHUB_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->Common.IsFDO)
    {
        DPRINT1("Usbhub: FDO stub for major function 0x%lx\n",
            IoGetCurrentIrpStackLocation(Irp)->MajorFunction);
        return ForwardIrpAndForget(DeviceObject, Irp);
    }
    else
    {
        //
        // Cant forward as we are the PDO!
        //
        DPRINT1("USBHUB: ERROR- PDO stub for major function 0x%lx\n",
            IoGetCurrentIrpStackLocation(Irp)->MajorFunction);
#ifndef NDEBUG
        DbgBreakPoint();
#endif
    }

    Status = Irp->IoStatus.Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}


NTSTATUS NTAPI
USBHUB_DispatchDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    DPRINT1("Usbhub: DispatchDeviceControl\n");
    if (((PHUB_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->Common.IsFDO)
        return USBHUB_FdoHandleDeviceControl(DeviceObject, Irp);
    else
        return USBHUB_IrpStub(DeviceObject, Irp);
}

NTSTATUS NTAPI
USBHUB_DispatchInternalDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    //DPRINT1("Usbhub: DispatchInternalDeviceControl\n");
    if (((PHUB_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->Common.IsFDO)
        return USBHUB_IrpStub(DeviceObject, Irp);
    else
        return USBHUB_PdoHandleInternalDeviceControl(DeviceObject, Irp);
}

NTSTATUS NTAPI
USBHUB_DispatchPnp(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    DPRINT1("USBHUB: DispatchPnp\n");
    if (((PHUB_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->Common.IsFDO)
        return USBHUB_FdoHandlePnp(DeviceObject, Irp);
    else
        return USBHUB_PdoHandlePnp(DeviceObject, Irp);
}

NTSTATUS NTAPI
USBHUB_DispatchPower(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp)
{
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_SUPPORTED;
}

VOID
NTAPI
USBHUB_Unload(
    IN PDRIVER_OBJECT DriverObject)
{
    UNIMPLEMENTED
}


NTSTATUS NTAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
    DriverObject->DriverExtension->AddDevice = USBHUB_AddDevice;
    DriverObject->DriverUnload = USBHUB_Unload;

    DPRINT1("USBHUB: DriverEntry\n");

    DriverObject->MajorFunction[IRP_MJ_CREATE] = USBHUB_Create;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = USBHUB_Close;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = USBHUB_Cleanup;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = USBHUB_DispatchDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = USBHUB_DispatchInternalDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = USBHUB_DispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] =USBHUB_DispatchPower;

    return STATUS_SUCCESS;
}

