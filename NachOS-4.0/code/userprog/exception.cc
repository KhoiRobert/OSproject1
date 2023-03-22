// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------
void IncreasePC()
{
	int counter = kernel->machine->ReadRegister(PCReg);
	kernel->machine->WriteRegister(PrevPCReg, counter);
	counter = kernel->machine->ReadRegister(NextPCReg);
	kernel->machine->WriteRegister(PCReg, counter);
	kernel->machine->WriteRegister(NextPCReg, counter + 4);
}

char *User2System(int virtAddr, int limit)
{
	int i; // index
	int oneChar;
	char *kernelBuf = NULL;

	kernelBuf = new char[limit + 1]; // need for terminal string
	if (kernelBuf == NULL)
		return kernelBuf;
	memset(kernelBuf, 0, limit + 1);

	// printf("\n Filename u2s:");
	for (i = 0; i < limit; i++)
	{
		kernel->machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuf[i] = (char)oneChar;
		// printf("%c",kernelBuf[i]);
		if (oneChar == 0)
			break;
	}
	return kernelBuf;
}

int System2User(int virtAddr, int len, char *buffer)
{
	if (len < 0)
		return -1;
	if (len == 0)
		return len;
	int i = 0;
	int oneChar = 0;
	do
	{
		oneChar = (int)buffer[i];
		kernel->machine->WriteMem(virtAddr + i, 1, oneChar);
		i++;
	} while (i < len && oneChar != 0);
	return i;
}

void ExceptionHandler(ExceptionType which)
{
	int type = kernel->machine->ReadRegister(2);

	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

	switch (which)
	{
	case SyscallException:
		switch (type)
		{
		case SC_Halt:
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

			SysHalt();

			ASSERTNOTREACHED();
			break;

		case SC_Add:
			DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");

			/* Process SysAdd Systemcall*/
			int result;
			result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
							/* int op2 */ (int)kernel->machine->ReadRegister(5));

			DEBUG(dbgSys, "Add returning with " << result << "\n");
			/* Prepare Result */
			kernel->machine->WriteRegister(2, (int)result);

			/* Modify return point */
			{
				/* set previous programm counter (debugging only)*/
				kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

				/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
				kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

				/* set next programm counter for brach execution */
				kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}

			return;

			ASSERTNOTREACHED();

			break;
		case SC_Create:
			int virAddrCreate;
			char *filename;
			virAddrCreate = kernel->machine->ReadRegister(4);
			filename = User2System(virAddrCreate, 32 + 1);
			if (strlen(filename) == 0)
			{
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				break;
			}
			if (filename == NULL)
			{
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				break;
			}
			if (!(kernel->fileSystem->Create(filename, 0)))
			{
				kernel->machine->WriteRegister(2, -1);
				IncreasePC();
				break;
			}
			kernel->machine->WriteRegister(2, 0);
			delete filename;
			IncreasePC();
			// break;
			return;
		case SC_Open:
			int virAddrOpen;
			int type;
			char *filenameOpen;
			int freeSlot;
			virAddrOpen = kernel->machine->ReadRegister(4);
			type = kernel->machine->ReadRegister(5);
			filenameOpen = User2System(virAddrOpen, 33);
			freeSlot = kernel->fileSystem->FindFreeSlot();
			if (freeSlot != -1)
				if (type == 0 || type == 1)
				{
					if ((kernel->fileSystem->openf[freeSlot] = kernel->fileSystem->Open(filenameOpen, type)) != NULL) // Mo file thanh cong
					{
						kernel->machine->WriteRegister(2, freeSlot); // tra ve OpenFileID
					}
					else if (type == 2)
					{
						kernel->machine->WriteRegister(2, 0);
					}
					else
					{
						kernel->machine->WriteRegister(2, 1);
					}
					delete[] filenameOpen;
					break;
				}
			kernel->machine->WriteRegister(2, -1);
			delete[] filenameOpen;
			break;
		case SC_Close:
			// Input id cua file(OpenFileID)
			//  Output: 0: thanh cong, -1 that bai
			int fid;
			fid = kernel->machine->ReadRegister(4); // Lay id cua file tu thanh ghi so 4
			if (fid >= 0 && fid <= 14)				// Chi xu li khi fid nam trong [0, 14]
			{
				if (kernel->fileSystem->openf[fid]) // neu mo file thanh cong
				{
					delete kernel->fileSystem->openf[fid]; // Xoa vung nho luu tru file
					kernel->fileSystem->openf[fid] = NULL; // Gan vung nho NULL
					kernel->machine->WriteRegister(2, 0);
					break;
				}
			}
			kernel->machine->WriteRegister(2, -1);
			break;
		// case SC_Read:
		// 	int virtAddrRead;
		// 	int charcountRead;
		// 	int idRead;
		// 	int OldPos;
		// 	int NewPos;
		// 	char *buf;
		// 	virtAddrRead = kernel->machine->ReadRegister(4);  // Lay dia chi cua tham so buffer tu thanh ghi so 4
		// 	charcountRead = kernel->machine->ReadRegister(5); // Lay charcount tu thanh ghi so 5
		// 	idRead = kernel->machine->ReadRegister(6);		  // Lay id cua file tu thanh ghi so 6
			
		// 	// Kiem tra id cua file truyen vao co nam ngoai bang mo ta file khong
		// 	if (idRead < 0 || idRead > 14)
		// 	{
		// 		printf("\nKhong the read vi id nam ngoai bang mo ta file.");
		// 		kernel->machine->WriteRegister(2, -1);
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	// Kiem tra file co ton tai khong
		// 	if (kernel->fileSystem->openf[idRead] == NULL)
		// 	{
		// 		printf("\nKhong the read vi file nay khong ton tai.");
		// 		kernel->machine->WriteRegister(2, -1);
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	if (kernel->fileSystem->openf[idRead]->type == 3) // Xet truong hop doc file stdout (type quy uoc la 3) thi tra ve -1
		// 	{
		// 		printf("\nKhong the read file stdout.");
		// 		kernel->machine->WriteRegister(2, -1);
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	OldPos = kernel->fileSystem->openf[idRead]->GetCurrentPos(); // Kiem tra thanh cong thi lay vi tri OldPos
		// 	buf = User2System(virtAddrRead, charcountRead);			 // Copy chuoi tu vung nho User Space sang System Space voi bo dem buffer dai charcount
		// 	// Xet truong hop doc file stdin (type quy uoc la 2)
		// 	if (kernel->fileSystem->openf[idRead]->type == 2)
		// 	{
		// 		// Su dung ham Read cua lop SynchConsole de tra ve so byte thuc su doc duoc
		// 		int size = 0;
		// 		System2User(virtAddrRead, size, buf); // Copy chuoi tu vung nho System Space sang User Space voi bo dem buffer co do dai la so byte thuc su
		// 		kernel->machine->WriteRegister(2, size);  // Tra ve so byte thuc su doc duoc
		// 		delete buf;
		// 		IncreasePC();
		// 		return;
		// 	}
		// 	// Xet truong hop doc file binh thuong thi tra ve so byte thuc su
		// 	if ((kernel->fileSystem->openf[idRead]->Read(buf, charcountRead)) > 0)
		// 	{
		// 		// So byte thuc su = NewPos - OldPos
		// 		NewPos = kernel->fileSystem->openf[idRead]->GetCurrentPos();
		// 		// Copy chuoi tu vung nho System Space sang User Space voi bo dem buffer co do dai la so byte thuc su
		// 		System2User(virtAddrRead, NewPos - OldPos, buf);
		// 		kernel->machine->WriteRegister(2, NewPos - OldPos);
		// 	}
		// 	else
		// 	{
		// 		// Truong hop con lai la doc file co noi dung la NULL tra ve -2
		// 		// printf("\nDoc file rong.");
		// 		kernel->machine->WriteRegister(2, -2);
		// 	}
		// 	delete buf;
		// 	IncreasePC();
		// 	return;
		default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}
		break;
	default:
		cerr << "Unexpected user mode exception" << (int)which << "\n";
		break;
	}
	ASSERTNOTREACHED();
}
