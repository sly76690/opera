#pragma once
// OPERA engine: the algorithms, pitch, modulation, feedback, envelopes, sixteen voices and the sustain pedal.
static bool fmTests(){
 auto require=[](bool ok,const char* what){if(!ok)std::cerr<<"FM: "<<what<<"\n";return ok;};
 for(const auto& a:opera::algorithms()){bool heard=false;for(int i=0;i<6;++i){heard|=a.carrier[size_t(i)];for(int m=0;m<6;++m)if(a.from[size_t(i)]&(1u<<m))if(!require(m>i,"a modulator must come above what it modulates"))return false;}
  if(!require(heard,"every algorithm is heard"))return false;}
 constexpr double rate=48000;constexpr int fs=96000;
 auto sound=[](int algorithm){opera::Settings s;s.algorithm=algorithm;for(auto& o:s.op){o.level=0;o.time={.002f,.5f,1.f,.05f};o.level4={1,1,1,0};o.velocity=0;}s.op[0].level=1;return s;};
 auto play=[&](opera::Settings s,int note,int samples,int releaseAt=-1){opera::Synth synth;synth.prepare(2*rate);synth.noteOn(note,1.f,s);std::vector<float> x(static_cast<size_t>(samples));
  for(int i=0;i<samples;++i){if(i==releaseAt)synth.noteOff(note,s);x[size_t(i)]=synth.render(s);}return x;};
 auto crossings=[](const std::vector<float>& x,int from,int to){int n=0;for(int i=std::max(1,from);i<to;++i)if(x[size_t(i-1)]<0&&x[size_t(i)]>=0)++n;return n;};
 auto bright=[](const std::vector<float>& x,int from,int to){double d=0,l=0;for(int i=from+1;i<to;++i){const double a=x[size_t(i)]-x[size_t(i-1)];d+=a*a;l+=double(x[size_t(i)])*x[size_t(i)];}return d/std::max(l,1e-12);};
 auto energy=[](const std::vector<float>& x,int from,int to){double e=0;for(int i=from;i<to;++i)e+=double(x[size_t(i)])*x[size_t(i)];return e/double(to-from);};
 {const auto x=play(sound(13),69,fs);if(!require(std::abs(crossings(x,fs/10,fs)-396)<=3,"pitch: A4 is 440 Hz"))return false;}
 {auto s=sound(0);const auto plain=play(s,57,fs/2);s.op[1].level=.7f;const auto fm=play(s,57,fs/2);if(!require(bright(fm,fs/10,fs/2)>bright(plain,fs/10,fs/2)*3,"a modulator brightens its carrier"))return false;}
 {auto s=sound(13);s.op[0].level=0;s.op[5].level=1;const auto pure=play(s,57,fs/2);s.feedback=1;const auto fed=play(s,57,fs/2);if(!require(bright(fed,fs/10,fs/2)>bright(pure,fs/10,fs/2)*3,"feedback brightens operator 6"))return false;}
 {auto s=sound(13);s.op[0].time={.002f,.3f,.3f,.2f};s.op[0].level4={1,.3f,.1f,0};const auto x=play(s,57,fs*2,fs);
  if(!require(energy(x,fs/2,fs)<energy(x,0,fs/20)*.1&&energy(x,fs+fs/2,fs*2)<1e-6,"envelope: decay to level 3, release to silence"))return false;}
 {opera::Synth synth;synth.prepare(2*rate);auto s=sound(0);for(int n=0;n<17;++n)synth.noteOn(40+n,.8f,s);synth.render(s);if(!require(synth.sounding()==16,"sixteen voices, the seventeenth steals one"))return false;}
 {opera::Synth synth;synth.prepare(2*rate);auto s=sound(13);s.op[0].time={.002f,.5f,1.f,.01f};synth.noteOn(60,1,s);synth.sustain(true,s);synth.noteOff(60,s);for(int i=0;i<fs/2;++i)synth.render(s);
  const bool held=synth.sounding()==1;synth.sustain(false,s);for(int i=0;i<fs/2;++i)synth.render(s);if(!require(held&&synth.sounding()==0,"the sustain pedal holds, and lets go"))return false;}
 std::cout<<"PASS FM engine: 16 algorithms, pitch, modulation, feedback, envelopes, 16 voices, sustain\n";return true;
}
