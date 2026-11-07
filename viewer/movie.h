// SHAR PSP: проигрыватель родных роликов. Интро (Роб 28.09): после заставки GONE Makers — логотипы создателей и
// ролик оригинала, потом главное меню. Данные: build/movies/*.smv из tools/movie_export.py (формат SMV1 описан там):
// кадры JPEG 480x272 (картинка 4:3 без растяжки, по бокам размытый фон из неё же — правка Роба 49; рамки поверх нет,
// main.c movieDrawTv рисует её только в отрицательном контроле) + PCM s16 44,1 кГц стерео, по куску на кадр.
//
// Порядок при запуске оригинала — исполняемый файл диска PAL SLES_518.97 (строки и адреса, не код):
//   BootupContext ставит в очередь vuglogo -> foxlogo -> gracie -> radlogo (чтения таблицы имён 0x148c54, 0x148cac,
//   0x148d04, 0x148d5c; таблица роликов 0x426034), после загрузки фронтенда экран PlayMovie берёт intro (0x1b57b8);
//   demo.rmv — ролик простоя меню, fmv* — сюжет и галерея ViewMovies (0x1c2f10). Эталон PCSX2 build/ps2ref/boot:
//   Fox -> Radical -> LOADING -> меню.
//
// Кадры разжимает аппаратный sceJpeg (Media Engine); куски читает с карты свой поток (как загрузчик клеток), звук —
// свой канал sceAudio. Часы — сэмплы, отданные в канал: на экране кадр часы / 1764; нехватка данных = пауза, не рассинхрон.
// Без звука в файле часы — таймер. Все вызовы из главного потока.
#ifndef SHAR_MOVIE_H
#define SHAR_MOVIE_H

enum { MV_OK, MV_EFILE, MV_EHDR, MV_EMEM, MV_ETHREAD, MV_EAUDIO, MV_EREAD, MV_EDEC };

typedef struct {
  int frames;               // кадров в файле
  int shown, dropped;       // разжато и показано / пропущено из-за опоздания
  int late;                 // проходов, когда нужный кадр ещё не был прочитан
  int underruns;            // звук: блоков тишины из-за нехватки данных
  unsigned audio;           // сэмплов звука отдано в канал (часы)
  float decMsMax, readMsMax, readKBs;   // худшее разжатие кадра / чтение куска, мс; средняя скорость чтения
  int stride;               // шаг строки выхода sceJpeg в пикселях (512 или 480, определяется по первому кадру)
  int err;                  // MV_*
} MovieStats;

typedef struct Movie Movie;

// 1: загрузить avcodec и открыть sceJpeg 480x272 (0 = ок, <0 — нет аппаратного JPEG, интро пропускается); 0: закрыть
int  movieSystem(int on);
extern int movieVolume;                // сверка g09b-menu круг 2: громкость звука ролика 0..32767 (по умолчанию максимум); ставить перед movieOpen (main_frontend.h movieVolSync)
// NULL — нет файла, испорчен или нет памяти (errOut, если не NULL, получает MV_*); сразу стартуют чтение и звук
extern void (*movieNote)(const char *);   // №1206: строка о повторе открытия ролика
Movie *movieOpen(const char *fn, int *errOut);
int  movieUpdate(Movie *m);           // раз в кадр развёртки: разжать нужный кадр; 1 = идёт, 0 = кончился или ошибка
int  movieFrame(const Movie *m);      // номер кадра на экране (-1 — ещё ни одного)
void movieDraw(const Movie *m);       // между sceGuStart и sceGuFinish: чёрный экран + кадр 480x272 1:1
void moviePause(Movie *m, int on);    // служебное (снимок смоука): часы, звук и разжатие стоят
void movieClose(Movie *m, MovieStats *st);   // потоки остановлены, канал звука отпущен, память свободна

// разбор SMV1: те же проверки в movieOpen, в потоке чтения и на Маке (tests/movie/host.c); 0 = целый, -1 = испорчен
typedef struct { unsigned frames, total, maxChunk, idxOff, dataOff, flags, size, icrc; } MvHdr;
int mvHeader(const unsigned char *h, unsigned fileSize, MvHdr *o);     // 64 байта заголовка (crc, поля, размер файла)
int mvIndex(const MvHdr *o, const unsigned char *idx);                 // (кадры+1) смещений: crc, границы, размеры кусков
int mvChunk(const MvHdr *o, const unsigned char *c, unsigned len, unsigned *jl, unsigned *pcm);   // кусок: JPEG + PCM

#endif
