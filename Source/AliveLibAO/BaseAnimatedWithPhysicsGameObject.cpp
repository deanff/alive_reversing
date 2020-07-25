#include "stdafx_ao.h"
#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "Function.hpp"
#include "Map.hpp"
#include "stdlib.hpp"
#include "Shadow.hpp"
#include "Game.hpp"

START_NS_AO

BaseAnimatedWithPhysicsGameObject* BaseAnimatedWithPhysicsGameObject::ctor_417C10()
{
    ctor_487E10(1);

    SetVTable(&field_10_anim, 0x4BA2B8);
    SetVTable(this, 0x4BAA38);

    field_CC_bApplyShadows |= 1u;
    field_B0_path_number = gMap_507BA8.field_2_current_path;
    field_B2_lvl_number = gMap_507BA8.field_0_current_level;

    field_B4_velx = FP_FromInteger(0);
    field_A8_xpos = FP_FromInteger(0);
    field_B8_vely = FP_FromInteger(0);
    field_AC_ypos = FP_FromInteger(0);
    field_CA = 0;
    field_D0_pShadow = nullptr;
    field_C4_b = 105;
    field_C2_g = 105;
    field_C0_r = 105;
    field_BC_sprite_scale = FP_FromInteger(1);
    field_C6_scale = 1;
    field_C8 = 5;

    field_6_flags.Clear(Options::eCanExplode_Bit7);
    field_6_flags.Clear(Options::eInteractive_Bit8);

    field_6_flags.Set(Options::eIsBaseAnimatedWithPhysicsObj_Bit5);
    field_6_flags.Set(Options::eDrawable_Bit4);

    return this;
}

void BaseAnimatedWithPhysicsGameObject::Animation_Init_417FD0(int /*frameTableOffset*/, int /*maxW*/, int /*maxH*/, BYTE** /*ppAnimData*/, __int16 /*a6*/)
{
    NOT_IMPLEMENTED();
}

void BaseAnimatedWithPhysicsGameObject::VRender_417DA0(int** /*ot*/)
{
    NOT_IMPLEMENTED();
}

void BaseAnimatedWithPhysicsGameObject::SetTint_418750(const TintEntry* /*pTintArray*/, LevelIds /*levelId*/)
{
    NOT_IMPLEMENTED();
}

BaseGameObject* BaseAnimatedWithPhysicsGameObject::dtor_417D10()
{
    SetVTable(this, 0x4BAA38);

    if (!field_6_flags.Get(BaseGameObject::eListAddFailed_Bit1))
    {
        if (field_6_flags.Get(BaseGameObject::eDrawable_Bit4))
        {
            gObjList_drawables_504618->Remove_Item(this);
            field_10_anim.vCleanUp();
        }

        if (field_D0_pShadow)
        {
            field_D0_pShadow->dtor_462030();
            ao_delete_free_447540(field_D0_pShadow);
        }
    }
    return dtor_487DF0();
}

__int16 BaseAnimatedWithPhysicsGameObject::SetBaseAnimPaletteTint_4187C0(TintEntry* /*pTintArray*/, LevelIds /*level_id*/, int /*resourceID*/)
{
    NOT_IMPLEMENTED();
    return 0;
}

void BaseAnimatedWithPhysicsGameObject::VStackOnObjectsOfType_418930(unsigned __int16 /*typeToFind*/)
{
    NOT_IMPLEMENTED();
}


EXPORT CameraPos BaseAnimatedWithPhysicsGameObject::Is_In_Current_Camera_417CC0()
{
    PSX_RECT rect = {};
    VGetBoundingRect(&rect, 1);
    return gMap_507BA8.Rect_Location_Relative_To_Active_Camera_4448C0(&rect, 0);
}

END_NS_AO
