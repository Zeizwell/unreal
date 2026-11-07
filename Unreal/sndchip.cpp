#include "std.h"
#include "sysdefs.h"
#include "emul.h"
#include "vars.h"
#include "emul_2203.h"
#include "sndchip.h"
#include <math.h>

/*
   YM-2149F emulator for Unreal Speccy project
   created under public domain license by SMT, jan.2006
*/

unsigned SNDCHIP::render(AYOUT *src, unsigned srclen, unsigned clk_ticks, bufptr_t dst)
{
   start_frame(dst);
   for (unsigned index = 0; index < srclen; index++) {
      // if (src[index].timestamp > clk_ticks) continue; // wrong input data leads to crash
      select(src[index].reg_num);
      write(src[index].timestamp, src[index].reg_value);
   }
   return end_frame(clk_ticks);
}


const unsigned MULT_C_1 = 14; // fixed point precision for 'system tick -> ay tick'
// b = 1+ln2(max_ay_tick/8) = 1+ln2(max_ay_fq/8 / min_intfq) = 1+ln2(10000000/(10*8)) = 17.9
// assert(b+MULT_C_1 <= 32)

void SNDCHIP::start_frame(bufptr_t dst)
{
   r13_reloaded = 0;
   SNDRENDER::start_frame(dst);
}

unsigned SNDCHIP::end_frame(unsigned clk_ticks)
{
   // adjusting 't' with whole history will fix accumulation of rounding errors

   u64 end_chip_tick = ((passed_clk_ticks + clk_ticks) * chip_clock_rate) / system_clock_rate;

   flush( (unsigned) (end_chip_tick - passed_chip_ticks) );
   if (conf.sound.trinity) trin_frame();   // ZX Trinity: PWM sweep, once per frame
   unsigned res = SNDRENDER::end_frame(t);

   passed_clk_ticks += clk_ticks;
   passed_chip_ticks += t; t = 0;
   nextfmtickfloat = 0.; //Alone Coder
   nextfmtick = 0; //Alone Coder

   return res;
}

void SNDCHIP::flush(unsigned chiptick) // todo: noaction at (temp.sndblock || !conf.sound.ay)
{
   while (t < chiptick) {
      t++;
      if (++ta >= fa) ta = 0, bitA ^= -1;
      if (++tb >= fb) tb = 0, bitB ^= -1;
      if (++tc >= fc) tc = 0, bitC ^= -1;
      if (++tn >= fn)
         tn = 0,
         ns = (ns*2+1) ^ (((ns>>16)^(ns>>13)) & 1),
         bitN = 0 - ((ns >> 16) & 1);
      if (++te >= fe) {
         te = 0, env += denv;
         if (env & ~31) {
            unsigned mask = (1<<r.env);
            if (mask & ((1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<9)|(1<<15)))
               env = denv = 0;
            else if (mask & ((1<<8)|(1<<12)))
               env &= 31;
            else if (mask & ((1<<10)|(1<<14)))
               denv = -denv, env = env + denv;
            else env = 31, denv = 0; //11,13
         }
      }

      if (conf.sound.trinity) trin_tick();   // ZX Trinity timers

      unsigned en, mix_l, mix_r;

      en = ((ea & env) | va) & ((bitA | bit0) & (bitN | bit3));
      if (conf.sound.trinity && (trin[0].reg[0] & 3) == 1) en = en * trin[0].lvl / 15;   // SID
      mix_l  = vols[0][en]; mix_r  = vols[1][en];

      en = ((eb & env) | vb) & ((bitB | bit1) & (bitN | bit4));
      if (conf.sound.trinity && (trin[1].reg[0] & 3) == 1) en = en * trin[1].lvl / 15;
      mix_l += vols[2][en]; mix_r += vols[3][en];

      en = ((ec & env) | vc) & ((bitC | bit2) & (bitN | bit5));
      if (conf.sound.trinity && (trin[2].reg[0] & 3) == 1) en = en * trin[2].lvl / 15;
      mix_l += vols[4][en]; mix_r += vols[5][en];
//YM2203 here
      if (/*temp.sndblock ||*/ conf.sound.ay_chip == CHIP_YM2203) {
        if (t >= nextfmtick) {
          nextfmtickfloat += ayticks_per_fmtick;
          nextfmtick = (int)nextfmtickfloat;
          if (++FMbufN == FMBUFSIZE) {
            YM2203UpdateOne(Chip2203, FMbufs/*&FMbuf*/, FMBUFSIZE/*1*/);
            FMbufN = 0;
          };
          if (fmsoundon0 == 0) {
            //FMbufOUT=(int)(FMbuf*conf.sound.ay/8192*0.7f);
            FMbufOUT=((((i16)FMbufs[FMbufN])*FMbufMUL)>>16);
          }
          else FMbufOUT=0;
        }
        mix_l += FMbufOUT; mix_r += FMbufOUT;
      }; //Alone Coder
//
      if ((mix_l ^ SNDRENDER::mix_l) | (mix_r ^ SNDRENDER::mix_r)) // similar check inside update()
         update(t, mix_l, mix_r);
   }
}

void SNDCHIP::select(u8 nreg)
{
   if (chiptype == CHIP_AY) nreg &= 0x0F;
   if (nreg != 0xC0) trin_key = 0;   // ZX Trinity: key "TRIN" - only in a row into #C0
   activereg = nreg;
}

void SNDCHIP::write(unsigned timestamp, u8 val)
{
   if (activereg >= 0x20 && conf.sound.ay_chip == CHIP_YM2203)
   {
      if (timestamp) flush((timestamp * mult_const) >> MULT_C_1); // cputick * ( (chip_clock_rate/8) / system_clock_rate );
      if (activereg >= 0x2d && activereg <= 0x2f) {
         int oldayfq=Chip2203->OPN.ST.SSGclock /*ayfq*/;
         YM2203Write(Chip2203,0,activereg);
         YM2203Write(Chip2203,1,val);
         if (oldayfq!=Chip2203->OPN.ST.SSGclock) {
            //if (!conf.sound.ay_samples) flush(cpu.t);
            //ayfq=Chip2203->OPN.ST.SSGclock;
            //t=(unsigned)((__int64)t*ayfq/oldayfq);
            //mult_const2 = ((ayfq/conf.intfq) << (MULT_C_1-3))/conf.frame;
            //mult_const3 = TICK_F/2+(unsigned)((__int64)temp.snd_frame_ticks*conf.intfq*(1<<(MULT_C+3))/ayfq);
            //ay_div = ((unsigned)((double)ayfq*0x10*(double)SAMPLE_T/(double)conf.sound.fq));
            //ay_div2 = (ayfq*0x100)/(conf.sound.fq/32);
            set_timings(system_clock_rate,Chip2203->OPN.ST.SSGclock,SNDRENDER::sample_rate);
         }
      }
      else
      {
         YM2203Write(Chip2203,0,activereg);
         YM2203Write(Chip2203,1,val);
      }
      return;
   } //Dexus

   if (activereg >= 0x10) {
      if (conf.sound.trinity) trin_write(timestamp, val);   // ZX Trinity
      return;
   }

   if ((1 << activereg) & ((1<<1)|(1<<3)|(1<<5)|(1<<13))) val &= 0x0F;
   if ((1 << activereg) & ((1<<6)|(1<<8)|(1<<9)|(1<<10))) val &= 0x1F;

   if (activereg != 13 && reg[activereg] == val) return;

   reg[activereg] = val;


   if (timestamp) flush((timestamp * mult_const) >> MULT_C_1); // cputick * ( (chip_clock_rate/8) / system_clock_rate );

   switch (activereg) {
      case 0:
      case 1:
         fa = r.fA;
         break;
      case 2:
      case 3:
         fb = r.fB;
         break;
      case 4:
      case 5:
         fc = r.fC;
         break;
      case 6:
         fn = val*2;
         break;
      case 7:
         bit0 = 0 - ((val>>0) & 1);
         bit1 = 0 - ((val>>1) & 1);
         bit2 = 0 - ((val>>2) & 1);
         bit3 = 0 - ((val>>3) & 1);
         bit4 = 0 - ((val>>4) & 1);
         bit5 = 0 - ((val>>5) & 1);
         break;
      case 8:
         ea = (val & 0x10)? -1 : 0;
         va = ((val & 0x0F)*2+1) & ~ea;
         break;
      case 9:
         eb = (val & 0x10)? -1 : 0;
         vb = ((val & 0x0F)*2+1) & ~eb;
         break;
      case 10:
         ec = (val & 0x10)? -1 : 0;
         vc = ((val & 0x0F)*2+1) & ~ec;
         break;
      case 11:
      case 12:
         fe = r.envT;
         break;
      case 13:
         r13_reloaded = 1;
         te = 0;
         if (r.env & 4) env = 0, denv = 1; // attack
         else env = 31, denv = -1; // decay
         break;
   }
}

u8 SNDCHIP::read()
{
   u8 v;
   if (conf.sound.trinity && trin_read(v)) return v;   // ZX Trinity
   if (activereg >= 0x10) return 0xFF;
   return reg[activereg & 0x0F];
}

void SNDCHIP::set_timings(unsigned system_clock_rate, unsigned chip_clock_rate, unsigned sample_rate)
{
   if (conf.sound.ay_chip == CHIP_YM2203) { //install YM2203 frequencies
         Chip2203->OPN.ST.clock = conf.sound.ayfq*2;
         Chip2203->OPN.ST.rate = conf.sound.fq /*44100*/;
         OPNPrescaler_w(&Chip2203->OPN, 1 , 1 );
         //ayfq=Chip2203->OPN.ST.SSGclock;
         //????????? ????????? ????????? ????????? ayfq ???????????? ????????? "??????????????????????????????" ?????????????????????, ????????????????????? ??? ??????????????? ??????????????? ?????? ??????????????????.
         chip_clock_rate=Chip2203->OPN.ST.SSGclock;
   } //Dexus
   
   chip_clock_rate /= 8;

   SNDCHIP::system_clock_rate = system_clock_rate;
   SNDCHIP::chip_clock_rate = chip_clock_rate;

   mult_const = (unsigned) (((u64)chip_clock_rate << MULT_C_1) / system_clock_rate);
   SNDRENDER::set_timings(chip_clock_rate, sample_rate);
   passed_chip_ticks = passed_clk_ticks = 0;
   t = 0; ns = 0xFFFF;

   nextfmtickfloat = 0.; //Alone Coder
   nextfmtick = 0; //Alone Coder
   ayticks_per_fmtick = (float)chip_clock_rate/conf.sound.fq /*44100*/; //Alone Coder
   FMbufMUL=(u16)(((float)conf.sound.ay_vol/8192 /* =0..1 */)*0.1f*65536); //Alone Coder 0.36.4

   //apply_regs();
}

void SNDCHIP::set_volumes(unsigned global_vol, const SNDCHIP_VOLTAB *voltab, const SNDCHIP_PANTAB *stereo)
{
   for (int j = 0; j < 6; j++)
      for (int i = 0; i < 32; i++)
         vols[j][i] = (unsigned) (((u64)global_vol * voltab->v[i] * stereo->raw[j])/(65535*15*3));
}

void SNDCHIP::reset(unsigned timestamp)
{
   for (int i = 0; i < 14; i++) reg[i] = 0;
   trin_reset();

   if (Chip2203) YM2203ResetChip((void*)Chip2203); //Dexus
/*
   ayfq=Chip2203->OPN.ST.SSGclock; //Dexus
   mult_const2 = ((ayfq/conf.intfq) << (MULT_C_1-3))/conf.frame; //Dexus
   mult_const3 = TICK_F/2+(unsigned)((__int64)temp.snd_frame_ticks*conf.intfq*(1<<(MULT_C+3))/ayfq); //Dexus
   ay_div = ((unsigned)((double)ayfq*0x10*(double)SAMPLE_T/(double)conf.sound.fq)); //Dexus
   ay_div2 = (ayfq*0x100)/(conf.sound.fq/32); //Dexus
*/
   apply_regs(timestamp);
}

void SNDCHIP::apply_regs(unsigned timestamp)
{
   for (u8 r = 0; r < 16; r++) {
      select(r); u8 p = reg[r];
      /* clr cached values */
      write(timestamp, p ^ 1); write(timestamp, p);
   }
}

SNDCHIP::SNDCHIP()
{
   bitA = bitB = bitC = 0;
   nextfmtick = 0; //Alone Coder
   set_timings(SNDR_DEFAULT_SYSTICK_RATE, SNDR_DEFAULT_AY_RATE, SNDR_DEFAULT_SAMPLE_RATE);
   Chip2203 = (YM2203 *) YM2203Init(0, 0, conf.sound.ayfq*2, conf.sound.fq /*44100*/); //Dexus
   set_chip(CHIP_YM);
   set_volumes(0x7FFF, SNDR_VOL_YM, SNDR_PAN_ABC);
   reset();
}

// corresponds enum CHIP_TYPE
const char * const ay_chips[] = { "AY-3-8910", "YM2149F", "YM2203" }; //Dexus


const SNDCHIP_VOLTAB SNDR_VOL_AY_S =
{ { 0x0000,0x0000,0x0340,0x0340,0x04C0,0x04C0,0x06F2,0x06F2,0x0A44,0x0A44,0x0F13,0x0F13,0x1510,0x1510,0x227E,0x227E,
    0x289F,0x289F,0x414E,0x414E,0x5B21,0x5B21,0x7258,0x7258,0x905E,0x905E,0xB550,0xB550,0xD7A0,0xD7A0,0xFFFF,0xFFFF } };

const SNDCHIP_VOLTAB SNDR_VOL_YM_S =
{ { 0x0000,0x0000,0x00EF,0x01D0,0x0290,0x032A,0x03EE,0x04D2,0x0611,0x0782,0x0912,0x0A36,0x0C31,0x0EB6,0x1130,0x13A0,
    0x1751,0x1BF5,0x20E2,0x2594,0x2CA1,0x357F,0x3E45,0x475E,0x5502,0x6620,0x7730,0x8844,0xA1D2,0xC102,0xE0A2,0xFFFF } };

const SNDCHIP_PANTAB SNDR_PAN_MONO_S =
{ 100,100, 100,100, 100,100 };

const SNDCHIP_PANTAB SNDR_PAN_ABC_S =
{ 100,10,  66,66,   10,100 };

const SNDCHIP_PANTAB SNDR_PAN_ACB_S =
{ 100,10,  10,100,  66,66 };

const SNDCHIP_PANTAB SNDR_PAN_BAC_S =
{ 66,66,   100,10,  10,100 };

const SNDCHIP_PANTAB SNDR_PAN_BCA_S =
{ 10,100,  100,10,  66,66 };

const SNDCHIP_PANTAB SNDR_PAN_CAB_S =
{ 66,66,   10,100,  100,10 };

const SNDCHIP_PANTAB SNDR_PAN_CBA_S =
{ 10,100,  66,66,   100,10 };

// =========================================================================
// ZX Trinity: timer effects card (protocol - ZX Trinity SDK)
// Per chip: #C0 control - key "TRIN" (4 writes) opens, 0 closes, any write resyncs the stream;
// read #C0 - 'T' while open. #C1 - packet stream [code][data], applied when the last byte comes:
//   ROW   (1): channel 0..2, FX row 14 bytes, sweep shape (table number), start phase, keep flag
//   TABLE (2): number 0..255, 16 values
//   RESET (3): channel mask - new note: timers, steps and sweep (unless keep) from the start
//   INFO  (4): then read #C1: 'T','R','I','N', version
// Timer of an effect ticks every P chip ticks (8*P AY clocks), P = tone * 2^(-semi/12) + raw,
// always following the channel tone. Each step lasts P (SID with 2 steps and PWM - 2P split by the duty).
// =========================================================================
enum { TF_MIX = 3, TF_SID = 1, TF_SYNC = 2, TF_FM = 4, TF_EFM = 8, TF_FMPER = 32, TF_EFMPER = 64 };
enum { TP_ROW = 1, TP_TABLE = 2, TP_RESET = 3, TP_INFO = 4 };

static double trin_mul[256];        // 2^(-s/12) for semitones s = -128..127 (index - s as u8)
static bool trin_mul_ok = false;

static double trin_semi(u8 s)
{
   if (!trin_mul_ok) {
      for (int i = 0; i < 256; i++) trin_mul[i] = pow(2.0, -(double)(signed char)i / 12.0);
      trin_mul_ok = true;
   }
   return trin_mul[s];
}

static unsigned trin_clamp(double p, unsigned max)
{
   if (p < 1.0) return 1;
   if (p > max) return max;
   return (unsigned)(p + 0.5);
}

void SNDCHIP::trin_reset()
{
   trin_on = trin_key = trin_pos = trin_need = trin_info = 0;
   memset(trin, 0, sizeof trin);
   memset(trin_tab, 0, sizeof trin_tab);
   for (int c = 0; c < 3; c++) trin[c].lvl = 15;
}

// tone period of channel c from AY registers
unsigned SNDCHIP::trin_tone(int c)
{
   unsigned p = (c == 0 ? r.fA : c == 1 ? r.fB : r.fC) & 0x0FFF;
   return p ? p : 1;
}

// timer period of effect k of channel c (chip ticks): always follows the tone
unsigned SNDCHIP::trin_period(int c, int k)
{
   TRIN_CH &ch = trin[c];
   double p = trin_tone(c) * trin_semi(ch.reg[3 + 3 * k]) + (signed char)ch.reg[4 + 3 * k];
   return trin_clamp(p, 0xFFFF);
}

// length of effect k (1..16 steps): Mix - LEN1 3..0, FM - LEN1 7..4, Env FM - LEN2 3..0
u8 SNDCHIP::trin_len(int c, int k)
{
   const u8 *g = trin[c].reg;
   return (k == 0 ? g[12] & 15 : k == 1 ? g[12] >> 4 : g[13] & 15) + 1;
}

// apply current step of effect k and set its length
void SNDCHIP::trin_apply(int c, int k)
{
   TRIN_CH &ch = trin[c];
   TRIN_FX &fx = ch.fx[k];
   u8 len = trin_len(c, k);
   if (fx.step >= len) fx.step = 0;
   u8 v = trin_tab[ch.reg[5 + 3 * k]][fx.step];
   unsigned p = trin_period(c, k);
   fx.dur = p;

   if (k == 0) {
      if ((ch.reg[0] & TF_MIX) == TF_SID) {
         ch.lvl = v & 15;
         if (len == 2 && ch.reg[1]) {               // PWM: 2 steps share 2P by the duty
            double lo = (ch.reg[1] & 15) / 30.0, hi = (ch.reg[1] >> 4) / 30.0, d = hi;   // 0..50%; no sweep - max
            if (ch.reg[2]) d = lo + (hi - lo) * trin_sweep_pos(ch);
            unsigned h = trin_clamp(2.0 * p * d, 2 * p - 1);
            fx.dur = fx.step == 0 ? h : 2 * p - h;
         }
      } else if ((ch.reg[0] & TF_MIX) == TF_SYNC) {  // envelope restart, step - shape (0 - current)
         if (v & 15) r.env = v & 15;
         r13_reloaded = 1;
         te = 0;
         if (r.env & 4) env = 0, denv = 1;
         else env = 31, denv = -1;
      }
   } else if (k == 1) {                              // FM: tone period of the channel
      unsigned t0 = trin_tone(c);
      unsigned np = ch.reg[0] & TF_FMPER ? trin_clamp((double)t0 + (signed char)v, 0x0FFF)
                                         : trin_clamp(t0 * trin_semi(v), 0x0FFF);
      if (c == 0) fa = np; else if (c == 1) fb = np; else fc = np;
   } else {                                          // Env FM: envelope period
      unsigned e0 = r.envT ? r.envT : 1;
      fe = ch.reg[0] & TF_EFMPER ? trin_clamp((double)e0 + (signed char)v, 0xFFFF)
                                 : trin_clamp(e0 * trin_semi(v), 0xFFFF);
   }
}

// next step of effect k
void SNDCHIP::trin_next(int c, int k)
{
   trin[c].fx[k].step++;
   trin_apply(c, k);
}

// every chip tick: timers of active effects
void SNDCHIP::trin_tick()
{
   for (int c = 0; c < 3; c++) {
      u8 f = trin[c].reg[0];
      if (!(f & (TF_MIX | TF_FM | TF_EFM))) continue;
      if ((f & TF_MIX) && ++trin[c].fx[0].cnt >= trin[c].fx[0].dur) trin[c].fx[0].cnt = 0, trin_next(c, 0);
      if ((f & TF_FM) && ++trin[c].fx[1].cnt >= trin[c].fx[1].dur) trin[c].fx[1].cnt = 0, trin_next(c, 1);
      if ((f & TF_EFM) && ++trin[c].fx[2].cnt >= trin[c].fx[2].dur) trin[c].fx[2].cnt = 0, trin_next(c, 2);
   }
}

// sweep position 0..1: shape - table of 16 curve points 0..255, linear between them (in a circle)
double SNDCHIP::trin_sweep_pos(const TRIN_CH &ch)
{
   const u8 *t = trin_tab[ch.shape];
   unsigned i = ch.sweep >> 12, f = ch.sweep & 0x0FFF;
   double a = t[i], b = t[(i + 1) & 15];
   return (a + (b - a) * f / 4096.0) / 255.0;
}

// once per frame: PWM sweep phase (256 steps per cycle, speed - added per frame)
void SNDCHIP::trin_log(u8 *p)
{
   for (int c = 0; c < 3; c++) {
      memcpy(p, trin[c].reg, TRIN_ROW), p += TRIN_ROW;
      *p++ = (u8)trin[c].sweep, *p++ = (u8)(trin[c].sweep >> 8);
      for (int k = 0; k < 3; k++) {
         unsigned per = trin_period(c, k);
         *p++ = (u8)per, *p++ = (u8)(per >> 8);
      }
   }
}

void SNDCHIP::trin_frame()
{
   for (int c = 0; c < 3; c++)
      if (trin[c].reg[2]) trin[c].sweep = (trin[c].sweep + trin[c].reg[2] * 256) & 0xFFFF;
}

// ROW: new FX row of channel c; effects switched off give the channel back to AY registers
void SNDCHIP::trin_row(int c, const u8 *row, u8 shape, u8 start, u8 keep)
{
   TRIN_CH &ch = trin[c];
   u8 was = ch.reg[0];
   memcpy(ch.reg, row, TRIN_ROW);
   ch.shape = shape, ch.start = start, ch.keep = keep & 1;
   u8 now = ch.reg[0];
   if ((was & TF_FM) && !(now & TF_FM)) {
      if (c == 0) fa = r.fA; else if (c == 1) fb = r.fB; else fc = r.fC;
   }
   if ((was & TF_EFM) && !(now & TF_EFM)) fe = r.envT;
   if (!(now & TF_MIX)) ch.lvl = 15;
   static const u8 bit[3] = { TF_MIX, TF_FM, TF_EFM };
   for (int k = 0; k < 3; k++)
      if ((now & bit[k]) && !(was & bit[k])) {      // just switched on - from the first step
         ch.fx[k].step = 0, ch.fx[k].cnt = 0;
         trin_apply(c, k);
      }
}

// a whole packet came: apply it
void SNDCHIP::trin_packet()
{
   const u8 *b = trin_pkt;
   switch (b[0]) {
      case TP_ROW:
         if (b[1] < 3) trin_row(b[1], b + 2, b[16], b[17], b[18]);
         break;
      case TP_TABLE:
         memcpy(trin_tab[b[1]], b + 2, TRIN_STEPS);
         break;
      case TP_RESET:
         for (int c = 0; c < 3; c++) {
            if (!(b[1] & (1 << c))) continue;
            TRIN_CH &ch = trin[c];
            static const u8 bit[3] = { TF_MIX, TF_FM, TF_EFM };
            for (int k = 0; k < 3; k++) {
               ch.fx[k].step = 0, ch.fx[k].cnt = 0;
               if (ch.reg[0] & bit[k]) trin_apply(c, k);
            }
            if (!ch.keep) ch.sweep = ch.start << 8;
         }
         break;
      case TP_INFO:
         trin_info = 1;
         break;
   }
}

bool SNDCHIP::trin_write(unsigned timestamp, u8 val)
{
   static const char key[] = "TRIN";
   u8 n = activereg;
   if (n == 0xC0) {
      trin_pos = 0;                                  // any write - stream from the start
      if (val == 0 && trin_key == 0) { trin_on = 0; return true; }
      if ((char)val == key[trin_key]) trin_key++;
      else trin_key = (char)val == key[0];
      if (trin_key == 4) trin_on = 1, trin_key = 0;
      return true;
   }
   if (!trin_on) return false;
   if (n != 0xC1) return false;
   if (timestamp) flush((timestamp * mult_const) >> MULT_C_1);
   if (trin_pos == 0) {                              // packet code -> its length
      static const u8 len[5] = { 0, 19, 18, 2, 1 };
      if (val == 0 || val > TP_INFO) return true;    // unknown code - skip the byte
      trin_need = len[val];
   }
   trin_pkt[trin_pos++] = val;
   if (trin_pos >= trin_need) trin_packet(), trin_pos = 0;
   return true;
}

// read: #C0 - 'T' (card is there, open); #C1 after INFO - "TRIN", version 2
bool SNDCHIP::trin_read(u8 &val)
{
   if (!trin_on) return false;
   if (activereg == 0xC0) { val = 'T'; return true; }
   if (activereg == 0xC1) {
      static const u8 info[5] = { 'T', 'R', 'I', 'N', 2 };
      val = trin_info >= 1 && trin_info <= 5 ? info[trin_info - 1] : 0;
      if (trin_info >= 1 && trin_info < 5) trin_info++; else trin_info = 0;
      return true;
   }
   return false;
}
