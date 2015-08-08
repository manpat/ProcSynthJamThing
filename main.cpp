#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <algorithm>

#include <fmod.hpp>
#include <fmod_errors.h>

#include <SDL2/SDL.h>

using std::cout; using std::endl;

struct NoteTimePair {
	float freq;
	float begin;
	float length;
};

constexpr float ntof(float n){
	return 220.f * std::pow(2.f, n/12.f);
}

struct Schedule {
	std::vector<NoteTimePair> notes;
	double time = 0.0f; // TODO: Separate time and phase. Repeat causes pop with phase
	double repeat = 0.0f;
	bool dirty = false;

	void Add(float freq, float when, float length = 1.0f){
		if(when+length < time) return;

		notes.push_back(NoteTimePair{freq, when, length});
	}

	void Update(float dt){
		time += dt;
		if(repeat > 0.0) time = std::fmod(time, repeat);
	}

	template<typename Func>
	void PlayNotes(Func f){
		for(auto& n: notes){
			if(n.begin > time) continue;
			if((n.begin+n.length) < time) continue;
			f(n);
		}
	}
};

struct Scale {
	std::vector<int> degrees;

	void Major(int root){
		auto d = 0;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 1;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 1;
	}

	void Minor(int root){
		auto d = 0;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 1;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 1;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
	}

	void Idk(int root){
		auto d = 0;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 1;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 1;
		degrees.push_back(root + d); d += 2;
	}

	void Penta(int root){
		auto d = 0;
		degrees.push_back(root + d); d += 3;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 2;
		degrees.push_back(root + d); d += 3;
		degrees.push_back(root + d); d += 2;
	}

	float Get(int degree){
		auto idx = degree;
		int octave = 0;
		while(idx < 0) {--octave; idx += degrees.size();}
		octave += idx/degrees.size();
		idx %= degrees.size();

		auto note = degrees[idx]+octave*12;
		return ntof(note);
	}
};

namespace Wave {
	float sin(double phase){
		return std::sin(2.0*M_PI*phase);
	}

	float saw(double phase){
		return fmod(phase, 2.0)-1.0;
	}

	float sqr(double phase, double width){
		auto nph = fmod(phase, 1.0);
		if(nph < width) return -1.0;

		return 1.0;
	}

	float tri(double phase){
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
			float x, y, z;
		};
		float v[3];
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

Scale cmaj;
Scale cmin;
Scale penta;
Scale scale;
Schedule sched;

int main(){
	SDL_Init(SDL_INIT_EVERYTHING);
	auto sdlWindow = SDL_CreateWindow("FMOD Test",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		200, 200, SDL_WINDOW_OPENGL);
	(void)sdlWindow;

	InitFmod();

	// vec3 forward{0, 0, 1.0f};
	// vec3 up{0, 1.0f, 0};
	// vec3 vel{0, 0, 0};

	cmaj.Major(0);
	cmin.Minor(0);
	penta.Penta(0);
	scale.Idk(0);

	bool running = true;
	double t = 0.0;
	// sched.time = 11.0;
	// sched.Add(ntof(-12), 0.0, 8.0);
	// sched.Add(ntof(0), 1.0);
	// sched.Add(ntof(12), 3.0);
	// sched.Add(ntof(7), 4.0);

	// sched.Add(ntof(9), 6.0);
	// sched.Add(ntof(14), 6.0);
	// sched.Add(ntof(10), 9.0);
	// sched.Add(ntof(15), 9.0);

	// auto chord = [&](auto& scale, int root, float when){
	// 	sched.Add(scale.Get(root+0), when+0.0, 3.0);
	// 	sched.Add(scale.Get(root+2), when+0.1, 3.0);
	// 	sched.Add(scale.Get(root+4), when+0.2, 3.0);
	// 	sched.Add(scale.Get(root+6), when+0.3, 3.0);
	// };

	// chord(cmaj, 0, 0.0);
	// chord(cmaj, 3, 3.0);
	// chord(cmaj, 1, 6.0);
	// chord(cmaj, 4, 9.0);

	// chord(cmin, 0, 12+0.0);
	// chord(cmin, 3, 12+3.0);
	// chord(cmin, 1, 12+6.0);
	// chord(cmin, 4, 12+9.0);

	sched.repeat = 7.0;

	// Bass
	for(float x = 0.0; x < sched.repeat;){
		x += std::pow(2.0, (rand()%3)-1.5);
		auto freq = penta.Get(rand()%(penta.degrees.size()*1)) * std::pow(2.0, -2.0);
		sched.Add(freq, x, 1.0);
		sched.Add(freq, x, 0.25);
		sched.Add(freq*2, x, 1.0);
		sched.Add(freq*2, x, 1.0);
	}

	// Mid
	for(float x = 0.0; x < sched.repeat;){
		x += std::pow(2.0, (rand()%4)-3.0);
		auto freq = penta.Get(rand()%(penta.degrees.size()*2)) * std::pow(2.0, -1.0);
		sched.Add(freq, x, 0.3);
		sched.Add(freq, x, 0.3);
	}

	// Treb
	for(float x = 0.0; x < sched.repeat;){
		x += std::pow(2.0, (rand()%4)-4.0);
		sched.Add(penta.Get(rand()%(penta.degrees.size()*3)) * std::pow(2.0, 1.0), x, 0.1 * std::pow(2.0, (rand()%2)-2.0));
	}

	// for(int i = 0; i <= 14; i++){
	// 	auto j = (i > 7)?(14-i):i;

	// 	sched.Add(cmin.Get(j), i*0.25);
	// }

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
		
		// constexpr float dist = 6.f;
		t += 0.000003;
		// vec3 pos = vec3{(float)cos(t)*(float)(dist + sin(t*0.3)*dist*0.5f), 0.f, (float)sin(t)*(float)(dist + sin(t*0.3)*dist*0.5f)};
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
FMOD_RESULT F_CALLBACK DSPCallback(FMOD_DSP_STATE* dsp_state, 
	float* inbuffer, float* outbuffer, uint length, 
	int inchannels, int* outchannels){

	assert(*outchannels >= 2);

	FMOD::DSP *thisdsp = (FMOD::DSP *)dsp_state->instance; 

	void* ud = nullptr;
	cfmod(thisdsp->getUserData(&ud));

	int samplerate = 0;
	cfmod(dsp_state->callbacks->getsamplerate(dsp_state, &samplerate));
	double inc = 1.0/samplerate;

	auto& sched = *static_cast<Schedule*>(ud);
	double& phase = sched.time;
	
	for(uint i = 0; i < length; i++){
		double out = 0.f;
		// if(std::fmod(phase, 0.5) > 0.25){
		// 	auto a = Wave::sqr(220.0*phase, 0.5);
		// 	auto b = Wave::sqr(110.0*phase, 0.5);

		// 	out += a + b;
		// }

		constexpr double attack = 0.1;

		sched.PlayNotes([&](NoteTimePair& n){
			auto env = (phase-n.begin)/n.length;
			if(env < attack){
				env /= attack;
			}else{
				env = (1.0-env)/(1.0-attack);
			}

			double o = 0.0;
			o += Wave::sin(n.freq*phase*0.5) * env;
			o += Wave::sin(n.freq*phase) * env;
			o += Wave::sin((n.freq)*phase+0.3) * env;
			out += o/3.0;
		});
		// auto c = Wave::sin( 55.0*phase);
		// auto d = Wave::sqr( 55.0*phase, 0.5);
		// out += c + d;

		outbuffer[i**outchannels+0] = out;
		// outbuffer[i**outchannels+1] = Wave::sin(110.0*phase)*0.2f;
		// phase += inc;
		sched.Update(inc);
	}

	return FMOD_OK;
}


void InitFmod(){
	cfmod(FMOD::System_Create(&fmodSystem));

	uint version = 0;
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

		strncpy(desc.name, "Fuckyou", sizeof(desc.name));
		desc.numinputbuffers = 0;
		desc.numoutputbuffers = 1;
		desc.read = DSPCallback;
		desc.userdata = &sched;

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