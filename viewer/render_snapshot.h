// Test-only A/B: restore the same retained renderer inputs, no game step between draws.
// Requested only by smoke flag rfcompare. Ordinary gameplay never allocates this snapshot.
static int rfCapture, rfBreak;
static int rf108Sv[3], rf108Skip;   // rf108Skip (flag rf108skip 1): пятый проход без прежних путей — основа для сравнения шума проходов   // 108: выключатели «прежний путь» (следы, картинки эффектов, рисунки меток) до проверки rfcompare
static Car rfSavedCar;
static Homer *rfSavedHomer;
static Traffic *rfSavedTraffic;
static SpRun *rfSavedSp;
static float *rfSavedJoints;
static unsigned rfJointBytes;
static unsigned rfHashBytes(unsigned h, const void *p, unsigned n) {
  const unsigned char *b = p; for (unsigned i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u; return h;
}
static void rfFreezeFree(void) {
  free(rfSavedHomer); free(rfSavedTraffic); free(rfSavedSp); free(rfSavedJoints);
  rfSavedHomer = 0; rfSavedTraffic = 0; rfSavedSp = 0; rfSavedJoints = 0; rfJointBytes = 0;
}
static int rfFreezeTake(void) {
  rfSavedCar = car;
  if (haveHomer) { rfSavedHomer = memalign(16, sizeof homer); if (!rfSavedHomer) goto fail; memcpy(rfSavedHomer, &homer, sizeof homer); }
  if (haveTraffic) { rfSavedTraffic = memalign(16, sizeof traffic); if (!rfSavedTraffic) goto fail; memcpy(rfSavedTraffic, &traffic, sizeof traffic); }
  if (haveSP && sprops.ninst) {
    rfSavedSp = malloc(sizeof(SpRun) * sprops.ninst); if (!rfSavedSp) goto fail;
    memcpy(rfSavedSp, sprops.run, sizeof(SpRun) * sprops.ninst);
    for (unsigned i = 0; i < sprops.ninst; i++) rfJointBytes += sprops.type[sprops.inst[i].type].nJoint * 12u * sizeof(float);
    rfSavedJoints = memalign(16, rfJointBytes ? rfJointBytes : 16); if (!rfSavedJoints) goto fail;
    unsigned off = 0; for (unsigned i = 0; i < sprops.ninst; i++) {
      unsigned n = sprops.type[sprops.inst[i].type].nJoint * 12u;
      memcpy(rfSavedJoints + off, sprops.run[i].jm, n * sizeof(float)); off += n;
    }
  }
  return 1;
fail: rfFreezeFree(); smkWrite("RF_ABORT snapshot-allocation\n"); return 0;
}
static void rfFreezeRestore(void) {
  car = rfSavedCar;
  if (rfSavedHomer) memcpy(&homer, rfSavedHomer, sizeof homer);
  if (rfSavedTraffic) memcpy(&traffic, rfSavedTraffic, sizeof traffic);
  if (rfSavedSp) {
    memcpy(sprops.run, rfSavedSp, sizeof(SpRun) * sprops.ninst);
    unsigned off = 0; for (unsigned i = 0; i < sprops.ninst; i++) {
      unsigned n = sprops.type[sprops.inst[i].type].nJoint * 12u;
      memcpy(sprops.run[i].jm, rfSavedJoints + off, n * sizeof(float)); off += n;
    }
  }
}
static unsigned rfFreezeHash(const float eye[6]) {
  unsigned h = rfHashBytes(2166136261u, &car, sizeof car);
  if (rfSavedHomer) h = rfHashBytes(h, &homer, sizeof homer);
  if (rfSavedTraffic) h = rfHashBytes(h, &traffic, sizeof traffic);
  if (rfSavedSp) {
    h = rfHashBytes(h, sprops.run, sizeof(SpRun) * sprops.ninst);
    for (unsigned i = 0; i < sprops.ninst; i++) h = rfHashBytes(h, sprops.run[i].jm, sprops.type[sprops.inst[i].type].nJoint * 12u * sizeof(float));
  }
  return rfHashBytes(h, eye, 6 * sizeof(float));
}
static int rfWriteSnapshot(SceUID fd, const void *p, unsigned n) {
  const unsigned char *b = p; while (n) { int wrote = sceIoWrite(fd, b, n); if (wrote <= 0) return 0; b += wrote; n -= wrote; } return 1;
}
static void rfFreezeDump(const float eye[6], long frame) {
  char path[64]; snprintf(path, sizeof path, "rfsnap_%s.bin", smkScene);
  SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
  if (fd < 0) { smkWrite("RF_ABORT snapshot-file\n"); return; }
  unsigned expected = 8 + sizeof frame + sizeof smkT + 24 + sizeof rfSavedCar +
    (rfSavedHomer ? sizeof homer : 0) + (rfSavedTraffic ? sizeof traffic : 0) +
    (rfSavedSp ? sizeof(SpRun) * sprops.ninst + rfJointBytes : 0);
  int ok = rfWriteSnapshot(fd, "RFSNAP1", 8) && rfWriteSnapshot(fd, &frame, sizeof frame) &&
    rfWriteSnapshot(fd, &smkT, sizeof smkT) && rfWriteSnapshot(fd, eye, 24) && rfWriteSnapshot(fd, &rfSavedCar, sizeof rfSavedCar);
  if (ok && rfSavedHomer) ok = rfWriteSnapshot(fd, rfSavedHomer, sizeof homer);
  if (ok && rfSavedTraffic) ok = rfWriteSnapshot(fd, rfSavedTraffic, sizeof traffic);
  if (ok && rfSavedSp) ok = rfWriteSnapshot(fd, rfSavedSp, sizeof(SpRun) * sprops.ninst) && rfWriteSnapshot(fd, rfSavedJoints, rfJointBytes);
  if (!ok) smkWrite("RF_ABORT snapshot-short-write\n");
  char log[200]; snprintf(log, sizeof log, "RF_SNAPSHOT sc=%s file=%s bytes=%u car=%u homer=%u traffic=%u instances=%u joints=%u ok=%d\n",
    smkScene, path, expected, (unsigned)sizeof car, rfSavedHomer ? (unsigned)sizeof homer : 0,
    rfSavedTraffic ? (unsigned)sizeof traffic : 0, rfSavedSp ? sprops.ninst : 0, rfJointBytes, ok); smkWrite(log);
  sceIoClose(fd);
}
