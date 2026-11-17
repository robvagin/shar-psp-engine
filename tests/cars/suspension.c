// Real coupled suspension function; independent active-set solution is in Python.
#include <stdio.h>
#include "../../viewer/car.c"
int main(void) {
  carSusp1278Old=1;   // 107b: тест проверяет решатель с нижним пределом 0 (прежняя модель); новая модель (тяга ниже нейтрали) — tests/car107b/susp1278.sh
  for(unsigned k=0;k<sizeof carBodyPhysics/sizeof *carBodyPhysics;k++)for(int pose=0;pose<3;pose++)for(int step=0;step<3;step++) {
    Car c;memset(&c,0,sizeof c);carParamsFor(carBodyPhysics[k].name,&c.p);derive(&c);
    c.q[0]=1;c.q[1]=.09f*pose;c.q[2]=-.12f*pose;c.q[3]=.06f*pose;quatNorm(c.q);M3 M=quatM(c.q);
    V3 r[4],n[4];float f[4],s[4]={0},t[4]={0},d[4],v[4]={-.4f,.1f,-.2f,.3f};int a[4]={1,1,1,1};float dt=1.f/(30.f*(1<<step));
    for(int j=0;j<4;j++) {
      r[j]=mulM(&M,v3((j&1?-1.f:1.f)*c.p.wheelX-c.comBase[0],-c.comBase[1],(j<2?c.p.wheelF:-c.p.wheelR)-c.comBase[2]));
      n[j]=v3(.05f*j,1.f,-.03f*j);n[j]=scl(n[j],1.f/len(n[j]));d[j]=c.cS[j]*13.f;
    }
    suspensionForces(&c,&M,r,n,s,t,d,v,a,dt,f);
#ifdef SUSPENSION_EXPLICIT_CONTROL
    for(int j=0;j<4;j++)f[j]=clampf(-d[j]*v[j],0.f,6.f*bodyMass(&c)*c.p.gravity);
#endif
    printf("%s %.9g",c.p.name,(double)dt);
    for(int j=0;j<4;j++)printf(" %.9g",(double)c.q[j]);
    for(int j=0;j<4;j++)printf(" %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g",(double)r[j].x,(double)r[j].y,(double)r[j].z,(double)n[j].x,(double)n[j].y,(double)n[j].z,(double)d[j],(double)v[j],(double)f[j]);
    printf("\n");
  }
}
