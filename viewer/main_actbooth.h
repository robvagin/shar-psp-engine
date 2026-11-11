// Блок △ у телефонной будки (правка 62, №682, №842, №1051, №1060), вынесен из main_loop.h без изменений: сверка g10b-loop круг 3 (P05:2) вызывает его дважды —
// раньше двери, гэга и выключателя (ранг будки 2) и на прежнем месте (ACTION_RANK_OLD=1). Включается внутрь функции кадра игры, не отдельно
    if (smkOn && (hit & PSP_CTRL_TRIANGLE)) { char s_[160]; snprintf(s_, sizeof s_, "BOOTHDBG tri haveH=%d haveC=%d fly=%d chord=%d onfoot=%d blocked=%d paused=%d pvOn=%d near=%d boothsOff=%d pbk=%d armed=%d t=%.1f\n", haveHomer, haveCol, fly, flyChord, onFoot(), mvBoothBlocked(), uiPaused(), phoneViewOn(), phoneNear(), mvBoothsOff(), pbArm.k, pbArm.armed, smkT); smkWrite(s_); }   // Build 96: почему △ не открыл будку (L6)
    if ((hit & PSP_CTRL_TRIANGLE) && haveHomer && haveCol && !fly && !flyChord && onFoot() && !(mvBoothBlocked() || uiPaused() || phoneViewOn())) {   // сверка g07 круг 5 (P05-25): не при проваленной миссии, аресте, открытой паузе и открытой будке   // правка 62: △ у будки — своя машина
      int pk = phoneNear(), pb_ = pbPress(&pbArm, pk, !mvBoothsOff());   // ps2-twin P05:25 (PHONEBUSY_OLD=1 — прежнее): будки выключены (миссия с принудительной машиной) — гудок «занято» и ничего не открывается; вошёл при выключенных — молча
      if (pb_ == PB_BUSY) { uiSfxPlayNamed("phonebusy"); if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "PHONE busy t=%.1f\n", smkT); smkWrite(s_); } hit &= ~PSP_CTRL_TRIANGLE; }
      else if (pk >= 0 && (pb_ == PB_OPEN)) { char sd[256]; levelFile(sd, ""); phoneK = pk;
        phoneViewSetProgress(svLvReached(&lvKeep));   // №682: пройденные уровни открывают свои машины в будке
        phoneCheatAll = !CHEATS1104_OLD && gameCheatOn(CH_VEHICLES);   // №1104: код Vehicles — все машины-награды в будке
        phoneViewSetCar(mvForcedCar(), car.destroyed ? 1.f : car.damage);   // правка 104: «Damaged: N %» у машины, на которой игрок сейчас
        Transit tb_; memset(&tb_, 0, sizeof tb_); tb_.name = "booth"; tb_.screen = TFS_PRELOADER; tb_.need = 320u << 10; tb_.at[0] = homer.x; tb_.at[1] = homer.y; tb_.at[2] = homer.z;
        if (!tfsm95Old && boothFsm95Old) tfExtBegin(&tb_);   // Build 95 П2: будка видна машине переходов (тело пока своё: ступени 1051, сцена, пересборка)
        if (!(!tfsm95Old && !boothFsm95Old && boothMem1051 && !PHONE_ROOM842_OLD && mem1060) && boothMem1051 && !PHONE_ROOM842_OLD) { unsigned b0_ = memMaxBlock ? memMaxBlock() : 0, f0_ = heapFreeBytes(); int n_ = boothRoom1051(0); if (n_) memLadNote("booth", 2, b0_, f0_, "pre"); }   // №1051: место — до первой попытки, не после неудачи (правка 435: ступень 2 лестницы)
        int fsmBooth_ = !tfsm95Old && !boothFsm95Old && boothMem1051 && !PHONE_ROOM842_OLD && mem1060;   // Build 95 П2: тело будки на ldrRun (transit_booth.h)
        int po_ = fsmBooth_ ? boothOpenFsm(sd, &fbp0, tb_.at) : phoneFailTest > 0 ? (phoneFailTest--, -1) : phoneViewOpen(mvShop(), sd);   // phoneFailTest — №842 тест
        if (!fsmBooth_ && po_ != 0 && boothMem1051 && !PHONE_ROOM842_OLD) {   // №1051: не вышло — ступенями больше места: дальше 20 м, потом все чужие клетки
          uiPakSegMax1051(256u << 10);   // ступени: пакет кусками по 256 КБ (тесная раздробленная куча)
          for (int st_ = 1; st_ <= 2 && po_ != 0; st_++) { unsigned b0_ = memMaxBlock ? memMaxBlock() : 0, f0_ = heapFreeBytes(); boothRoom1051(st_); memLadNote("booth", st_ == 1 ? 3 : 4, b0_, f0_, st_ == 1 ? "r20" : "all"); po_ = phoneViewOpen(mvShop(), sd); }
          uiPakSegMax1051(0);
          { char s_[96]; snprintf(s_, sizeof s_, "BOOTH1051 result ok=%d t=%.1f\n", po_ == 0, smkT); if (smkOn) smkWrite(s_); bbNote(s_); }
          if (po_ != 0 && mem1060) {   // №1060: четвёртая ступень — газета, всё в кучу, при дроблении пересборка уровня; «unavailable» из-за памяти не бывает
            { unsigned b0_ = memMaxBlock ? memMaxBlock() : 0, f0_ = heapFreeBytes();
              float at_[3] = {homer.x, homer.y, homer.z}; int fits_ = transitPlan("booth", 320u << 10, at_);   // Будка-93 (1233): общий вход перехода
              int direct_ = !booth1225Old && mem1060Rebuild && boothCells1225() <= 4 && (transitPlanOld ? b0_ < (320u << 10) : !fits_);   // Будка-93 (1225/1227): сцена нечего не освободит — сразу пересборка, один экран
              if (smkOn) { char s_[120]; snprintf(s_, sizeof s_, "BOOTH1225 plan direct=%d cells=%d blk=%u free=%u t=%.1f\n", direct_, boothCells1225(), b0_, f0_, smkT); smkWrite(s_); }
              if (direct_) { mem1060Fade(&fbp0); po_ = boothRebuild1060(sd, &fbp0); memLadNote("booth", 4, b0_, f0_, po_ == 0 ? "rebuild-ok" : "rebuild-fail"); if (po_ == 0) mem1060Back = 0; else mem1060Back = 1; }   // город свежий: возвращать нечего
              else { po_ = boothScene1060(sd, &fbp0); mem1060Back = 1; memLadNote("booth", 4, b0_, f0_, po_ == 0 ? "scene-ok" : "scene-fail");
                if (po_ != 0 && mem1060Rebuild) { b0_ = memMaxBlock ? memMaxBlock() : 0; f0_ = heapFreeBytes(); po_ = boothRebuild1060(sd, &fbp0); memLadNote("booth", 4, b0_, f0_, po_ == 0 ? "rebuild-ok" : "rebuild-fail"); if (po_ == 0 && !booth1225Old) mem1060Back = 0; } } }
            { char s_[96]; snprintf(s_, sizeof s_, "BOOTH1060 result ok=%d t=%.1f\n", po_ == 0, smkT); if (smkOn) smkWrite(s_); bbNote(s_); }
            if (po_ != 0) { memLadNote("booth", 5, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), "simplify"); boothRelease1051(); mem1060Return(&fbp0); } }
          else
          if (po_ != 0) boothRelease1051(); }
        else
        if (!fsmBooth_ && po_ != 0 && !PHONE_ROOM842_OLD) {   // №842: не влезла — клетки города дальше 50 м в кучу (будка закрывает экран) и ещё раз
          unsigned b0_ = memMaxBlock ? memMaxBlock() : 0; int nd_ = cellsDropBeyond(50.f); po_ = phoneViewOpen(mvShop(), sd);
          { char s_[160]; snprintf(s_, sizeof s_, "PHONE842 retry level=%d dropped=%d blk=%u->%u free=%u ok=%d t=%.1f\n", gameLevel, nd_, b0_, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), po_ == 0, smkT);
            if (smkOn) smkWrite(s_); bbNote(s_); } }
        if (!tfsm95Old && boothFsm95Old) tfExtEnd(&tb_, po_ == 0);
        if (po_ != 0) {
          // №375: ошибка пакета не должна подменять выбор автоматическим вызовом машины
          if (uiReady) uiMessage("TELEPHONE BOOTH UNAVAILABLE", -1, 3.f);
          if (smkOn) { char s_[320]; snprintf(s_, sizeof s_, "PHONE open-fail level=%d dir=%s t=%.1f\n", gameLevel, sd, smkT); smkWrite(s_); }
        }
        else { if (boothMem1051) { char s_[120]; snprintf(s_, sizeof s_, "BOOTH1051 open ok free=%.2f blk=%.2f t=%.1f\n", heapFreeBytes() / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f, smkT); if (smkOn) smkWrite(s_); bbNote(s_); }
          if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "PHONE view open t=%.1f\n", smkT); smkWrite(s_); } }
        hit &= ~PSP_CTRL_TRIANGLE; } }
