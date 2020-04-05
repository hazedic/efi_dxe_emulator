#include "events.h"
#include "efi_definitions.h"
#include "loader.h"
#include "logging.h"

#include <map>

struct EFI_EVENT_IMPL
{
    bool signaled;
    uint64_t notify_routine;
    uint64_t notify_context;
    /* Trampoline for the notification routine */
    uint64_t tramp[2];
};

static std::map<EFI_EVENT, EFI_EVENT_IMPL> g_events;

EFI_EVENT create_efi_event(uc_engine * uc, UINT32 Type, EFI_TPL NotifyTpl, EFI_EVENT_NOTIFY NotifyFunction, VOID* NotifyContext)
{
    static uint64_t g_counter = 1;

    auto notify_routine = reinterpret_cast<uint64_t>(NotifyFunction);
    auto notify_context = reinterpret_cast<uint64_t>(NotifyContext);

    EFI_EVENT_IMPL ei = { false, notify_routine, notify_context, 0, 0 };
    install_trampoline(uc, notify_routine, &ei.tramp[0], &ei.tramp[1]);
    
    g_events[reinterpret_cast<EFI_EVENT>(g_counter)] = ei;
    return reinterpret_cast<EFI_EVENT>(g_counter++);
}

void signal_efi_event(uc_engine* uc, EFI_EVENT Event)
{
    auto ei = g_events.find(Event);
    ei->second.signaled = true;
}

void dispatch_event_notification_routines(uc_engine* uc)
{
    for (const auto& ei : g_events)
    {
        if (ei.second.signaled)
        {
            DEBUG_MSG("Dispatching notification routine for event %p at %p", ei.first, ei.second.notify_routine);
            uc_reg_write(uc, UC_X86_REG_RCX, &ei.first);
            uc_reg_write(uc, UC_X86_REG_RDX, &ei.second.notify_context);
            uc_emu_start(uc, ei.second.tramp[0], ei.second.tramp[1], 0, 0);
        }
    }
}