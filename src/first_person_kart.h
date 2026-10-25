#ifndef FIRST_PERSON_KART_H
#define FIRST_PERSON_KART_H

#include "camera.h"

typedef struct {
    Vec3f pos;
    Vec3f right;
    Vec3f up;
    Vec3f forward;
    s16 bodyYaw;
} FirstPersonKartBodyPose;

s32 get_first_person_kart_body_pose(FirstPersonKartBodyPose* pose, const Player* player);
const u8* first_person_kart_color(s32 characterId);
void render_first_person_kart(Camera* camera, s32 screenIdx);

#endif
