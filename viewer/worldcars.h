// Машины мира: стоят сами (локатор CarStart уровня, не миссии), ставит движок при NEW GAME (main.c worldCarsPlace, правка 53).
// Модель каждой — в пакете msncars-l<N>.shr уровня: tools/msncar_export.py читает этот файл (правка 268: без модели машина
// стояла твёрдой и невидимой). Координаты движка (z отзеркален), heading — как у локатора
#ifndef SHAR_WORLDCARS_H
#define SHAR_WORLDCARS_H
// сверка g02-traffic круг 2 (g10b P03-20): «свободная машина» грузится с загрузкой зоны, уходит с выгрузкой, если игрок дальше WORLD_CAR_FAR_R, и уходит, когда миссия ставит свою машину ближе
// WORLD_CAR_MISSION_R. Здесь только числа; сама привязка (убрать/вернуть wcar[], game_msn.c) — окно g07/game_msn.c (передано)
#define WORLD_CAR_FAR_R 200.f
#define WORLD_CAR_MISSION_R 10.f
static const struct { int level; const char *name; float p[3]; float h; } WORLD_CAR_LIST[] = {
  {1, "rocke_v", {-90.2f, -0.77f, 211.6f}, 0.f},   // ракетная машина Гомера, локатор «rocke» l1r3.p3d (запись PS2 32:35), G10-1
#ifndef WORLD_CARS_OLD
  // №418 (Роб 09.10: «нет уникальных машин, которые стоят в одном месте; L5 J4-7 — Bigfoot»): на каждом уровне ровно один
  // CarStart-локатор с именем машины в данных (data[2..] = строка; у прочих 605 CarStart данных 2 слова) — ParkedCarManager
  // исполняемого файла ("FreeCar", "art\cars\%s.p3d"). Точка и курс — из локатора (z отзеркален), код места — viewer/placecode.h
  {2, "mono_v",  {-215.7f, 42.7f, -488.0f}, 0.f},          // l2z4.p3d «mono_v»     E3-3
  {3, "knigh_v", {-489.7f, -49.1f, -94.8f}, 1.5707964f},   // l3r3.p3d «knigh_v»    C7-1
  {4, "atv_v",   {410.1f, -2.3f, -481.1f}, -2.6179938f},   // l4z7.p3d «atv_v»      M3-1
  {5, "oblit_v", {113.2f, 14.7f, -311.9f}, 2.6179938f},    // l5r4.p3d «oblit_v»    J4-7 (у стадиона: большие колёса)
  {6, "hype_v",  {-493.0f, -59.3f, 69.5f}, 0.f},           // l6z3.p3d «hype_v»     C8-7
  {7, "dune_v",  {199.6f, 9.3f, 363.7f}, 2.2689281f},      // l7z2.p3d «dune_v»     J11-6
#endif
};
#endif
