#include "stdafx_ao.h"
#include "Alarm.hpp"
#include "Game.hpp"
#include "Map.hpp"
#include "DynamicArray.hpp"
#include "Function.hpp"
#include "SwitchStates.hpp"
#include "stdlib.hpp"
#include "CameraSwapper.hpp"

START_NS_AO

void Alarm_ForceLink() { }

ALIVE_VAR(1, 0x5076A8, short, alarmInstanceCount_5076A8, 0);

EXPORT Alarm * Alarm::ctor_402570(__int16 a2, __int16 switchId, __int16 a4, __int16 layer)
{
    ctor_461550(layer, 1);

    SetVTable(this, 0x4BA060);
    field_6C_15_timer = a4 + gnFrameCount_507670;
    field_74_switch_id = switchId;
    field_4_typeId = Types::eAlarm_1;
    field_68_r_value = 0;
    field_6A_state = 0;
    field_70_duration_timer = field_6C_15_timer + a2;

    alarmInstanceCount_5076A8++;
    if (alarmInstanceCount_5076A8 > 1)
    {
        field_6_flags.Set(BaseGameObject::eDead_Bit3);
    }

    field_5E_r = 0;
    field_60_g = 0;
    field_62_b = 0;

    // Disable red screen flashing in the stock yards
    if (gMap_507BA8.field_0_current_level == LevelIds::eStockYards_5 ||
        gMap_507BA8.field_0_current_level == LevelIds::eStockYardsReturn_6)
    {
        gObjList_drawables_504618->Remove_Item(this);
        field_6_flags.Clear(BaseGameObject::eDrawable_Bit4);
    }
    return this;
}

BaseGameObject* Alarm::dtor_402630()
{
    SetVTable(this, 0x4BA060);
    alarmInstanceCount_5076A8--;
    SwitchStates_Set(field_74_switch_id, 0);
    return dtor_461630();
}

BaseGameObject* Alarm::VDestructor(signed int flags)
{
    return Vdtor_402830(flags);
}

Alarm* Alarm::Vdtor_402830(signed int flags)
{
    dtor_402630();
    if (flags & 1)
    {
        ao_delete_free_447540(this);
    }
    return this;
}

void Alarm::VScreenChanged_402810()
{
    if (gMap_507BA8.field_28_cd_or_overlay_num != gMap_507BA8.GetOverlayId_4440B0())
    {
        field_6_flags.Set(BaseGameObject::eDead_Bit3);
    }
}

void Alarm::VRender_4027F0(int** ppOt)
{
    if (!sNumCamSwappers_507668)
    {
        EffectBase::VRender_461690(ppOt);
    }
}

void Alarm::VRender(int** ppOt)
{
    VRender_4027F0(ppOt);
}

void Alarm::VScreenChanged()
{
    VScreenChanged_402810();
}

END_NS_AO
