#include <algorithm>
#include <iostream>
#include <cstdint>
#include <cassert>
#include <vector>
#include <cmath>

#include <fmod.hpp>
#include <fmod_errors.h>

#include <SDL2/SDL.h>

using std::cout; using std::endl;

using u32 = uint32_t;
using s32 = int32_t;
using f32 = float;
using f64 = double;

struct NoteTimePair {
	f32 freq;
	f32 volume;
	f32 begin;
	f32 length;
};

// Returns frequency of note offset from A
constexpr f32 ntof(f32 n){
	return 220.f * std::pow(2.f, n/12.f);
}

struct Schedule {
	std::vector<NoteTimePair> notes;
	f64 time = 0.0f;
	f32 repeat = 0.0f;
	s32 numRepeats = 0;

	void Add(f32 freq, f32 when, f32 length = 1.0f, f32 volume = 1.0f){
		if(when+length < time) return;

		notes.push_back(NoteTimePair{freq, volume, when, length});
	}

	void Update(f64 dt){
		time += dt;
		if(repeat > 0.0) {
			if(time >= repeat){
				++numRepeats;
				time = std::fmod(time, repeat);
			}
		}
	}

	template<typename Func>
	void PlayNotes(Func f){
		for(auto& n: notes){
			if(n.begin >= time) continue;
			if((n.begin+n.length) <= time) continue;
			f(n);
		}
	}
};

enum Notes {
	A = 0, As, B, C, Cs, D, Ds, E, F, Fs, G, Gs
};

struct Scale {
	std::vector<s32> degrees;

	void Major(s32 root = Notes::A){
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 1;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 1;
	}

	void Minor(s32 root = Notes::A){
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 1;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 1;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
	}

	void Idk(s32 root = Notes::A){
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 1;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 1;
		degrees.push_back(root); root += 2;
	}

	void Penta(s32 root = Notes::A){
		degrees.push_back(root); root += 3;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 2;
		degrees.push_back(root); root += 3;
		degrees.push_back(root); root += 2;
	}

	f32 Get(s32 degree){
		auto idx = degree;
		s32 octave = 0;
		while(idx < 0) {--octave; idx += degrees.size();}
		octave += idx/degrees.size();
		idx %= degrees.size();

		auto note = degrees[idx]+octave*12;
		return ntof(note);
	}
};

namespace Wave {
	f32 sin(f64 phase){
		return std::sin(2.0*M_PI*phase);
	}

	f32 saw(f64 phase){
		return std::fmod(phase, 2.0)-1.0;
	}

	f32 sqr(f64 phase, f64 width = 0.5){
		auto nph = std::fmod(phase, 1.0);
		if(nph < width) return -1.0;

		return 1.0;
	}

	f32 tri(f64 phase){
		auto nph = std::fmod(phase, 1.0);
		if(nph <= 0.5) return (nph-0.25)*4.0;

		return (0.75-nph)*4.0;
	}
}

FMOD::System* fmodSystem;
FMOD::Channel* channel;
void InitFmod();

static void cfmod(FMOD_RESULT result) {
	if (result != FMOD_OK) {
		std::cerr << "FMOD error! (" << result << ") " << FMOD_ErrorString(result) << std::endl;
		throw "FMOD Error";
	}
}

Scale amaj;
Scale amin;
Scale penta;
Scale scale;
Schedule sched{};
Schedule perc{};
Schedule chords{};

constexpr f64 tempo = 120.0;

s32 main(s32, char**){
	SDL_Init(SDL_INIT_EVERYTHING);
	auto sdlWindow = SDL_CreateWindow("FMOD Test",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		200, 200, SDL_WINDOW_OPENGL);
	(void)sdlWindow;

	InitFmod();

	amaj.Major();
	amin.Minor();
	penta.Penta();
	scale.Idk();

	std::srand(1000);

	auto chord = [&](auto& scale, s32 root, f32 when, f32 vol = 3.f){
		constexpr f32 length = 3.5f;

		auto snd = root+1 + (rand()%2);
		auto trd = root+3 + (rand()%3);

		chords.Add(scale.Get(root)*0.5f, when+.00f, length, vol * 0.7f);
		// chords.Add(scale.Get(snd)*0.5f, when+.00f, length, vol * 0.7f);

		chords.Add(scale.Get(root), when+.02f, length, vol);
		chords.Add(scale.Get(snd), when+.04f, length, vol);
		chords.Add(scale.Get(trd), when+.06f, length, vol);
	};

	sched.time = -8.0; // Lead in
	sched.repeat = 7.5;

	chords.time = sched.time;
	chords.repeat = 8.0;
	perc.repeat = 8.0;

	// Kick
	for(f32 x = 0.0; x < perc.repeat; x+= 1.0){
		// perc.Add(50.0, x, 0.05, 10.0);
		perc.Add(30.0, x, 0.2, 4.0);
	}

	for(f32 x = 1.0; x < perc.repeat; x+= 2.0){
		perc.Add(1500.0, x, 0.01, 1.0);
	}
	// perc.Add(1500.0, 7.75, 0.01, 1.0);
	// perc.Add(1500.0, 6.75, 0.01, 1.0);

	auto gen = [&](){
		sched.notes.clear();

		// Bass
		for(f32 x = 0.0; x < sched.repeat;){
			x += std::pow(2.0, (rand()%2));
			auto freq = penta.Get(rand()%(penta.degrees.size()*1)) * std::pow(2.0, -2.0);
			// sched.Add(freq, x, 2.0/3.0, 6.0);
			sched.Add(freq, x, 1.0, 2.0);
			// sched.Add(freq*0.5, x+0.75, 2.0/3.0, 6.0);
		}

		// Mid
		for(f32 x = 0.0; x < sched.repeat;){
			x += std::pow(2.0, (rand()%4)-2.0);
			auto freq = penta.Get(rand()%(penta.degrees.size()*2));
			auto length = 0.3 * std::pow(2.0, (rand()%3)-1.0);
			auto vol = ((rand()%1000) - 500)/500.f + 2.0;
			sched.Add(freq, x, length, vol);
		}

		// Treb
		for(f32 x = 0.0; x < sched.repeat;){
			x += std::pow(2.0, (rand()%5)-2.0);
			auto freq = penta.Get(rand()%(penta.degrees.size()*3)) * std::pow(2.0, 1.0);
			auto length = 0.1 * std::pow(2.0, (rand()%4)-2.0);
			auto vol = ((rand()%1000) - 500)/1000.f + 1.0;

			sched.Add(freq, x, length, vol);
			sched.Add(freq, x+1.0/4.0, length, vol);
			// sched.Add(freq, x+2.0/4.0, length);
		}
	};

	auto genchords = [&]() {
		chords.notes.clear();

		for(f32 x = 0.f; x < chords.repeat;) {
			chord(penta, rand()%(penta.degrees.size()*5/4) - penta.degrees.size(), x, 0.8f + (rand()%100 - 50)/300.f);
			// x += std::pow(2.0, (rand()%2)+2) + (rand()%2)*0.5f;
			x += (rand()%5)/2.f + 2.f;
		}
	};

	gen();
	genchords();

	bool running = true;

	while(running){
		SDL_Event e;
		while(SDL_PollEvent(&e)){
			if (e.type == SDL_QUIT
			|| (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE)
			|| (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				running = false;
			}
		}

		fmodSystem->update();

		if(sched.numRepeats >= 4){
			gen();
			sched.numRepeats = 0;
		}

		if(chords.numRepeats >= 2){
			genchords();
			chords.numRepeats = 0;
		}

		SDL_Delay(10);
	}

	fmodSystem->release();
	SDL_Quit();

	return 0;
}

/*
	                                       
	88888888ba,    ad88888ba  88888888ba   
	88      `"8b  d8"     "8b 88      "8b  
	88        `8b Y8,         88      ,8P  
	88         88 `Y8aaaaa,   88aaaaaa8P'  
	88         88   `"""""8b, 88""""""'    
	88         8P         `8b 88           
	88      .a8P  Y8a     a8P 88           
	88888888Y"'    "Y88888P"  88           
	                                       
	                                       
*/
struct DSPUserdata {
	f64 phase;
};

FMOD_RESULT F_CALLBACK DSPCallback(FMOD_DSP_STATE* dsp_state, 
	f32* inbuffer, f32* outbuffer, u32 length, 
	s32 inchannels, s32* outchannels){

	assert(*outchannels >= 2);

	FMOD::DSP *thisdsp = (FMOD::DSP *)dsp_state->instance; 

	void* ud = nullptr;
	cfmod(thisdsp->getUserData(&ud));

	s32 samplerate = 0;
	cfmod(dsp_state->callbacks->getsamplerate(dsp_state, &samplerate));
	f64 inc = 1.0/samplerate;

	auto dud = static_cast<DSPUserdata*>(ud);
	auto& phase = dud->phase;

	for(u32 i = 0; i < length; i++){
		f32 out = 0.f;
		f32 outl = 0.f;
		f32 outr = 0.f;
		
		sched.PlayNotes([&](const NoteTimePair& n){
			constexpr f32 attack = 0.1;
			
			auto pos = (sched.time-n.begin)/n.length;
			f32 env;
			if(pos < attack){
				env = pos/attack;
			}else{
				env = (1.0-pos)/(1.0-attack);
			}

			f32 o = 0.0;
			// o += Wave::sin(n.freq*phase*0.5) * env * 0.2;
			// o += Wave::sin(n.freq*phase*2.0) * env;
			f32 mod = Wave::sin(phase*10.f) * .02f;
			f32 ph = n.freq*phase + mod;
			f32 a = std::min(1.f, std::max(env*env*env * .5f, 0.f)); //Wave::sin(phase*1.f)*.5f + .5f;

			env *= n.volume;
			o += (Wave::sin(ph) * (1-a) + Wave::sqr(ph*2.f) * a) * env;
			// o += Wave::sin((n.freq + Wave::tri(phase*6.f) * .01f)*phase) * env;
			// o += Wave::sin((n.freq + 0.5)*phase) * env;
			out += o/3.0;
		});

		chords.PlayNotes([&](const NoteTimePair& n){
			constexpr f32 attack = 0.005;
			
			auto pos = (chords.time-n.begin)/n.length;
			f32 env;
			if(pos < attack){
				env = pos/attack;
			}else{
				env = (1.0-pos)/(1.0-attack);
			}

			f32 mod = Wave::sin(phase*10.f) * .02f;
			f32 ph = n.freq*phase + mod;
			f32 a = env*env * .3f + .3f + Wave::sin(phase*6.f) * 0.2f;
			a = std::min(1.f, std::max(a, 0.f));

			f32 phaseShift = 0.2f + Wave::sin(phase*3.f) * .2f + .5f; //phase / 6.f;

			outl += (Wave::sin(ph) * (1-a) + Wave::tri(ph) * a) * env * n.volume;
			outr += (Wave::sin(ph + phaseShift) * (1-a) + Wave::tri(ph * 1.01) * a) * env * n.volume;
		});

		perc.PlayNotes([&](const NoteTimePair& n){
			constexpr f32 attack = 0.1;

			auto pos = (perc.time-n.begin)/n.length;
			f32 env = 0;
			if(pos < attack){
				env = pos/attack;
			}else{
				env = (1.0-pos)/(1.0-attack);
			}

			env *= n.volume;

			f32 o = 0;
			o += Wave::sin(n.freq*phase) * env;
			o += Wave::tri(n.freq*phase) * env;
			out += o;
		});

		outbuffer[i**outchannels+0] = out + outl/3.f;
		outbuffer[i**outchannels+1] = out + outr/3.f;

		phase += inc;
		chords.Update(inc/60.0* tempo);
		sched.Update(inc/60.0* tempo);
		perc.Update(inc/60.0* tempo);
	}

	return FMOD_OK;
}

void InitFmod(){
	cfmod(FMOD::System_Create(&fmodSystem));

	u32 version = 0;
	cfmod(fmodSystem->getVersion(&version));
	if(version < FMOD_VERSION){
		std::cerr 
			<< "FMOD version of at least " << FMOD_VERSION 
			<< " required. Version used " << version 
			<< std::endl;
		throw "FMOD Error";
	}

	cfmod(fmodSystem->init(100, FMOD_INIT_NORMAL, nullptr));

	FMOD::DSP* dsp;
	FMOD::DSP* compressor;

	{	FMOD_DSP_DESCRIPTION desc;
		memset(&desc, 0, sizeof(desc));

		// strncpy(desc.name, "Fuckyou", sizeof(desc.name));
		desc.numinputbuffers = 0;
		desc.numoutputbuffers = 1;
		desc.read = DSPCallback;
		desc.userdata = new DSPUserdata{/*sched, */0.0};

		cfmod(fmodSystem->createDSP(&desc, &dsp));
		cfmod(dsp->setChannelFormat(FMOD_CHANNELMASK_STEREO,2,FMOD_SPEAKERMODE_STEREO));
	}

	cfmod(fmodSystem->createDSPByType(FMOD_DSP_TYPE_COMPRESSOR, &compressor));

	cfmod(compressor->setParameterFloat(FMOD_DSP_COMPRESSOR_THRESHOLD, -13));
	cfmod(compressor->setParameterFloat(FMOD_DSP_COMPRESSOR_ATTACK, 1));
	cfmod(compressor->setBypass(false));
	cfmod(dsp->setBypass(false));

	FMOD::ChannelGroup* mastergroup;
	cfmod(fmodSystem->getMasterChannelGroup(&mastergroup));
	cfmod(mastergroup->addDSP(0, compressor));
	cfmod(fmodSystem->playDSP(dsp, mastergroup, false, &channel));
	cfmod(channel->setMode(FMOD_2D));
	cfmod(channel->setVolume(0.7f));

	FMOD::Reverb3D* reverb;
	cfmod(fmodSystem->createReverb3D(&reverb));

	// http://www.fmod.org/docs/content/generated/FMOD_REVERB_PROPERTIES.html

	FMOD_REVERB_PROPERTIES rprops = {
		.DecayTime			= 8000.0, //1500.0, /* Reverberation decay time in ms */
		.EarlyDelay			= 7.0, //7.0, /* Initial reflection delay time */
		.LateDelay			= 11.0, //11.0, /* Late reverberation delay time relative to initial reflection */
		.HFReference		= 5000.0, /* Reference high frequency (hz) */
		.HFDecayRatio		= 50.0, /* High-frequency to mid-frequency decay time ratio */
		.Diffusion			= 60.0, /* Value that controls the echo density in the late reverberation decay. */
		.Density			= 100.0, //100.0, /* Value that controls the modal density in the late reverberation decay */
		.LowShelfFrequency	= 250.0, /* Reference low frequency (hz) */
		.LowShelfGain		= 0.0, /* Relative room effect level at low frequencies */
		.HighCut			= 10000.0, /* Relative room effect level at high frequencies */
		.EarlyLateMix		= 50.0, /* Early reflections level relative to room effect */
		.WetLevel			= -12.0, //-6.0, /* Room effect level (at mid frequencies) */
	};

	cfmod(reverb->setProperties(&rprops));
}