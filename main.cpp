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
	f32 begin;
	f32 length;
};

// Returns frequency of note offset from A
constexpr f32 ntof(f32 n){
	return 220.f * std::pow(2.f, n/12.f);
}

struct Schedule {
	std::vector<NoteTimePair> notes;
	f32 time = 0.0f;
	f32 repeat = 0.0f;
	bool dirty = false;

	void Add(f32 freq, f32 when, f32 length = 1.0f){
		if(when+length < time) return;

		notes.push_back(NoteTimePair{freq, when, length});
	}

	void Update(f32 dt){
		time += dt;
		if(repeat > 0.0) time = std::fmod(time, repeat);
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
		return fmod(phase, 2.0)-1.0;
	}

	f32 sqr(f64 phase, f64 width){
		auto nph = fmod(phase, 1.0);
		if(nph < width) return -1.0;

		return 1.0;
	}

	f32 tri(f64 phase){
		auto nph = fmod(phase, 1.0);
		if(nph <= 0.5) return (nph-0.25)*4.0;

		return (1.0-nph -0.25)*4.0;
	}
}

FMOD::System* fmodSystem;
FMOD::Channel* channel;
void InitFmod();

struct vec3 {
	union {
		struct {
			f32 x, y, z;
		};
		f32 v[3];
	};

	operator FMOD_VECTOR() const {
		return FMOD_VECTOR{x,y,z};
	}
};

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
Schedule sched;

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
	// sched.time = 11.0;
	// sched.Add(ntof(-12), 0.0, 8.0);
	// sched.Add(ntof(0), 1.0);
	// sched.Add(ntof(12), 3.0);
	// sched.Add(ntof(7), 4.0);

	// sched.Add(ntof(9), 6.0);
	// sched.Add(ntof(14), 6.0);
	// sched.Add(ntof(10), 9.0);
	// sched.Add(ntof(15), 9.0);

	// auto chord = [&](auto& scale, s32 root, f32 when){
	// 	sched.Add(scale.Get(root+0), when+0.0, 3.0);
	// 	sched.Add(scale.Get(root+2), when+0.1, 3.0);
	// 	sched.Add(scale.Get(root+4), when+0.2, 3.0);
	// 	sched.Add(scale.Get(root+6), when+0.3, 3.0);
	// };

	// chord(amaj, 0, 0.0);
	// chord(amaj, 3, 3.0);
	// chord(amaj, 1, 6.0);
	// chord(amaj, 4, 9.0);

	// chord(amin, 0, 12+0.0);
	// chord(amin, 3, 12+3.0);
	// chord(amin, 1, 12+6.0);
	// chord(amin, 4, 12+9.0);

	sched.repeat = 7.0;

	// Bass
	for(f32 x = 0.0; x < sched.repeat;){
		x += std::pow(2.0, (rand()%3)-1.5);
		auto freq = penta.Get(rand()%(penta.degrees.size()*1)) * std::pow(2.0, -2.0);
		sched.Add(freq, x, 1.0);
		sched.Add(freq, x, 0.25);
		sched.Add(freq*2, x, 1.0);
		sched.Add(freq*2, x, 1.0);
	}

	// Mid
	for(f32 x = 0.0; x < sched.repeat;){
		x += std::pow(2.0, (rand()%4)-3.0);
		auto freq = penta.Get(rand()%(penta.degrees.size()*2)) * std::pow(2.0, -1.0);
		sched.Add(freq, x, 0.3);
		sched.Add(freq, x, 0.3);
	}

	// Treb
	for(f32 x = 0.0; x < sched.repeat;){
		x += std::pow(2.0, (rand()%4)-4.0);
		sched.Add(penta.Get(rand()%(penta.degrees.size()*3)) * std::pow(2.0, 1.0), x, 0.1 * std::pow(2.0, (rand()%2)-2.0));
	}

	bool running = true;
	// f64 t = 0.0;

	while(running){
		SDL_Event e;
		while(SDL_PollEvent(&e)){
			if(e.type == SDL_QUIT
			|| (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE)
			|| (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				running = false;
			}
		}

		fmodSystem->update();
		
		// constexpr f32 dist = 6.f;
		// t += 0.000003;
		// vec3 pos = vec3{(f32)cos(t)*(f32)(dist + sin(t*0.3)*dist*0.5f), 0.f, (f32)sin(t)*(f32)(dist + sin(t*0.3)*dist*0.5f)};
		// cfmod(fmodSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)&pos, (FMOD_VECTOR*)&vel, (FMOD_VECTOR*)&forward, (FMOD_VECTOR*)&up));
		// cfmod(channel->set3DAttributes((FMOD_VECTOR*)&pos, (FMOD_VECTOR*)&vel, nullptr));
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
	Schedule& sched;
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
	auto& sched = dud->sched;
	auto& phase = dud->phase;

	constexpr f32 attack = 0.1;
	
	for(u32 i = 0; i < length; i++){
		f32 out = 0.f;
		
		sched.PlayNotes([&](NoteTimePair& n){
			auto env = (sched.time-n.begin)/n.length;
			if(env < attack){
				env /= attack;
			}else{
				env = (1.0-env)/(1.0-attack);
			}

			f32 o = 0.0;
			o += Wave::sin(n.freq*phase*0.5) * env;
			o += Wave::sin(n.freq*phase) * env;
			o += Wave::sin((n.freq)*phase+0.3) * env;
			out += o/3.0;
		});

		outbuffer[i**outchannels+0] = out;
		// outbuffer[i**outchannels+1] = Wave::sin(110.0*phase)*0.2f;

		phase += inc;
		sched.Update(inc);
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
	{
		FMOD_DSP_DESCRIPTION desc;
		memset(&desc, 0, sizeof(desc));

		// strncpy(desc.name, "Fuckyou", sizeof(desc.name));
		desc.numinputbuffers = 0;
		desc.numoutputbuffers = 1;
		desc.read = DSPCallback;
		desc.userdata = new DSPUserdata{sched, 0.0};

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
	cfmod(channel->setMode(FMOD_3D));

	FMOD::Reverb3D* reverb;
	cfmod(fmodSystem->createReverb3D(&reverb));

	// http://www.fmod.org/docs/content/generated/FMOD_REVERB_PROPERTIES.html

	FMOD_REVERB_PROPERTIES rprops = {
		20000.0, //1500.0, /* Reverberation decay time in ms */
		10.0, //7.0, /* Initial reflection delay time */
		11.0, //11.0, /* Late reverberation delay time relative to initial reflection */
		5000.0, /* Reference high frequency (hz) */
		50.0, /* High-frequency to mid-frequency decay time ratio */
		100.0, /* Value that controls the echo density in the late reverberation decay. */
		50.0, //100.0, /* Value that controls the modal density in the late reverberation decay */
		250.0, /* Reference low frequency (hz) */
		0.0, /* Relative room effect level at low frequencies */
		20000.0, /* Relative room effect level at high frequencies */
		50.0, /* Early reflections level relative to room effect */
		-5.0, //-6.0, /* Room effect level (at mid frequencies) */
	};

	cfmod(reverb->setProperties(&rprops));
}