#include "stdafx.h"
#include "Emu/SysCalls/SysCalls.h"
#include "SC_Lwmutex.h"
#include "SC_Lwcond.h"

SysCallBase sys_lwcond("sys_lwcond");

int sys_lwcond_create(mem_ptr_t<sys_lwcond_t> lwcond, mem_ptr_t<sys_lwmutex_t> lwmutex, mem_ptr_t<sys_lwcond_attribute_t> attr)
{
	sys_lwcond.Warning("sys_lwcond_create(lwcond_addr=0x%x, lwmutex_addr=0x%x, attr_addr=0x%x)",
		lwcond.GetAddr(), lwmutex.GetAddr(), attr.GetAddr());

	if (!lwcond.IsGood() || !lwmutex.IsGood() || !attr.IsGood()) return CELL_EFAULT;

	lwcond->lwmutex = lwmutex.GetAddr();
	lwcond->lwcond_queue = sys_lwcond.GetNewId(new LWCond(attr->name_u64));

	sys_lwcond.Warning("*** lwcond created [%s] (attr=0x%x, lwmutex.sq=0x%x): id = %d", 
		wxString(attr->name, 8).wx_str(), (u32)lwmutex->attribute, (u32)lwmutex->sleep_queue, (u32)lwcond->lwcond_queue);
	return CELL_OK;
}

int sys_lwcond_destroy(mem_ptr_t<sys_lwcond_t> lwcond)
{
	sys_lwcond.Warning("sys_lwcond_destroy(lwcond_addr=0x%x)", lwcond.GetAddr());

	if (!lwcond.IsGood()) return CELL_EFAULT;
	LWCond* lwc;
	u32 id = (u32)lwcond->lwcond_queue;
	if (!sys_lwcond.CheckId(id, lwc)) return CELL_ESRCH;

	Emu.GetIdManager().RemoveID(id);
	return CELL_OK;
}

int sys_lwcond_signal(mem_ptr_t<sys_lwcond_t> lwcond)
{
	sys_lwcond.Log("sys_lwcond_signal(lwcond_addr=0x%x)", lwcond.GetAddr());

	if (!lwcond.IsGood()) return CELL_EFAULT;
	LWCond* lwc;
	u32 id = (u32)lwcond->lwcond_queue;
	if (!sys_lwcond.CheckId(id, lwc)) return CELL_ESRCH;

	lwc->signal(mem_ptr_t<sys_lwmutex_t>(lwcond->lwmutex)->attribute);

	return CELL_OK;
}

int sys_lwcond_signal_all(mem_ptr_t<sys_lwcond_t> lwcond)
{
	sys_lwcond.Log("sys_lwcond_signal_all(lwcond_addr=0x%x)", lwcond.GetAddr());

	if (!lwcond.IsGood()) return CELL_EFAULT;
	LWCond* lwc;
	u32 id = (u32)lwcond->lwcond_queue;
	if (!sys_lwcond.CheckId(id, lwc)) return CELL_ESRCH;

	lwc->signal_all();

	return CELL_OK;
}

int sys_lwcond_signal_to(mem_ptr_t<sys_lwcond_t> lwcond, u32 ppu_thread_id)
{
	sys_lwcond.Log("sys_lwcond_signal_to(lwcond_addr=0x%x, ppu_thread_id=%d)", lwcond.GetAddr(), ppu_thread_id);

	if (!lwcond.IsGood()) return CELL_EFAULT;
	LWCond* lwc;
	u32 id = (u32)lwcond->lwcond_queue;
	if (!sys_lwcond.CheckId(id, lwc)) return CELL_ESRCH;

	if (!lwc->signal_to(ppu_thread_id)) return CELL_EPERM;

	return CELL_OK;
}

int sys_lwcond_wait(mem_ptr_t<sys_lwcond_t> lwcond, u64 timeout)
{
	sys_lwcond.Log("sys_lwcond_wait(lwcond_addr=0x%x, timeout=%lld)", lwcond.GetAddr(), timeout);

	if (!lwcond.IsGood()) return CELL_EFAULT;
	LWCond* lwc;
	u32 id = (u32)lwcond->lwcond_queue;
	if (!sys_lwcond.CheckId(id, lwc)) return CELL_ESRCH;
	const u32 tid = GetCurrentPPUThread().GetId();

	mem_ptr_t<sys_lwmutex_t> lwmutex(lwcond->lwmutex);

	if ((u32)lwmutex->owner.GetOwner() != tid) return CELL_EPERM; // caller must own this lwmutex
	lwc->begin_waiting(tid);

	u32 counter = 0;
	const u32 max_counter = timeout ? (timeout / 1000) : 20000;
	bool was_locked = true;
	do
	{
		if (Emu.IsStopped())
		{
			ConLog.Warning("sys_lwcond_wait(sq id=%d, ...) aborted", id);
			return CELL_ETIMEDOUT;
		}
		if (was_locked) lwmutex->unlock(tid);
		Sleep(1);
		if (was_locked = (lwmutex->trylock(tid) == CELL_OK))
		{
			if (lwc->check(tid))
			{
				return CELL_OK;
			}
		}

		if (counter++ > max_counter)
		{
			if (!timeout) 
			{
				sys_lwcond.Warning("sys_lwcond_wait(lwcond_addr=0x%x): TIMEOUT", lwcond.GetAddr());
				counter = 0;
			}
			else
			{
				lwc->stop_waiting(tid);
				return CELL_ETIMEDOUT;
			}
		}		
	} while (true);
}
