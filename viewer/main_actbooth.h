// Блок △ у телефонной будки (правка 62, №682, №842, №1051, №1060), вынесен из main_loop.h без изменений: сверка g10b-loop круг 3 (P05:2) вызывает его дважды —
// раньше двери, гэга и выключателя (ранг будки 2) и на прежнем месте (ACTION_RANK_OLD=1). Включается внутрь функции кадра игры, не отдельно
    if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && haveCol && !fly && !flyChord && onFoot() && !(mvBoothBlocked() || uiPaused() || phoneViewOn())) {   // сверка g07 круг 5 (P05-25): не при проваленной миссии, аресте, открытой паузе и открытой будке   // правка 62: △ у будки — своя машина
      int pk = phoneNear(); if (pk >= 0) { char sd[256]; levelFile(sd, ""); phoneK = pk;
        phoneViewSetProgress(svLvReached(&lvKeep));   // №682: пройденные уровни открывают свои машины в будке
        phoneViewSetCar(mvForcedCar(), car.destroyed ? 1.f : car.damage);   // правка 104: «Damaged: N %» у машины, на которой игрок сейчас
        if (boothMem1051 && !PHONE_ROOM842_OLD) boothRoom1051(0);   // №1051: место — до первой попытки, не после неудачи
        int po_ = phoneFailTest > 0 ? (phoneFailTest--, -1) : phoneViewOpen(mvShop(), sd);   // phoneFailTest — №842 тест
        if (po_ != 0 && boothMem1051 && !PHONE_ROOM842_OLD) {   // №1051: не вышло — ступенями больше места: дальше 20 м, потом все чужие клетки
          uiPakSegMax1051(256u << 10);   // ступени: пакет кусками по 256 КБ (тесная раздробленная куча)
          for (int st_ = 1; st_ <= 2 && po_ != 0; st_++) { boothRoom1051(st_); po_ = phoneViewOpen(mvShop(), sd); }
          uiPakSegMax1051(0);
          { char s_[96]; snprintf(s_, sizeof s_, "BOOTH1051 result ok=%d t=%.1f\n", po_ == 0, smkT); if (smkOn) smkWrite(s_); bbNote(s_); }
          if (po_ != 0 && mem1060) {   // №1060: четвёртая ступень — газета, всё в кучу, при дроблении пересборка уровня; «unavailable» из-за памяти не бывает
            po_ = boothScene1060(sd, &fbp0); mem1060Back = 1;
            if (po_ != 0 && mem1060Rebuild) po_ = boothRebuild1060(sd, &fbp0);
            { char s_[96]; snprintf(s_, sizeof s_, "BOOTH1060 result ok=%d t=%.1f\n", po_ == 0, smkT); if (smkOn) smkWrite(s_); bbNote(s_); }
            if (po_ != 0) { boothRelease1051(); mem1060Return(&fbp0); } }
          else
          if (po_ != 0) boothRelease1051(); }
        else
        if (po_ != 0 && !PHONE_ROOM842_OLD) {   // №842: не влезла — клетки города дальше 50 м в кучу (будка закрывает экран) и ещё раз
          unsigned b0_ = memMaxBlock ? memMaxBlock() : 0; int nd_ = cellsDropBeyond(50.f); po_ = phoneViewOpen(mvShop(), sd);
          { char s_[160]; snprintf(s_, sizeof s_, "PHONE842 retry level=%d dropped=%d blk=%u->%u free=%u ok=%d t=%.1f\n", gameLevel, nd_, b0_, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), po_ == 0, smkT);
            if (smkOn) smkWrite(s_); bbNote(s_); } }
        if (po_ != 0) {
          // №375: ошибка пакета не должна подменять выбор автоматическим вызовом машины
          if (uiReady) uiMessage("TELEPHONE BOOTH UNAVAILABLE", -1, 3.f);
          if (smkOn) { char s_[320]; snprintf(s_, sizeof s_, "PHONE open-fail level=%d dir=%s t=%.1f\n", gameLevel, sd, smkT); smkWrite(s_); }
        }
        else { if (boothMem1051) { char s_[120]; snprintf(s_, sizeof s_, "BOOTH1051 open ok free=%.2f blk=%.2f t=%.1f\n", heapFreeBytes() / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f, smkT); if (smkOn) smkWrite(s_); bbNote(s_); }
          if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "PHONE view open t=%.1f\n", smkT); smkWrite(s_); } }
        hit &= ~PSP_CTRL_TRIANGLE; } }
