#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include "routine.h"
#include "think.h"
#include <math.h>
#include <cstdio>
#include "hardware/vreg.h"

const int sampleRate = 20000;

float globalMod = 0.f;

float noteToFreq(float note) {
	float a = 440.f;
	return (a / 32.f) * pow(2.f, ((note - 9.f) / 12.f));
}

inline float fmodFixed(float a, float b)
{
	return fmod(fmod(a, b) + b, b);
}

#define WAVE_SAW 0
#define WAVE_PLS 1
#define WAVE_SIN 2
#define WAVE_TRI 3
#define WAVE_SAMPLE 4
#define WAVE_SLICER 5

struct Oscilator
{
	Oscilator()
	{
		phase = 0.f;
		frequency = 0.f;
		volume = 0.f;
		dspVolume = .5f;
		dspFrequency = .5f;
		porta = 1.f;
		channelPorta = 1.f;
		slew = 0.01f;
		bend = 0;

		duty = .5f;

		masterVolume = 1;

		originalNote = 0;
		note = 0;
		noteOn = 0;

		sampleData = nullptr;
		sampleLength = 0;
		sampleCursor = 0;
		sampleSize = 0;

		type = WAVE_SAW;
	}

	/* void loadSample(std::string file)
	{
		std::ifstream fileD(file, std::ios::in | std::ios::binary | std::ios::ate);

		if (!fileD)
		{
			//std::cout << "Failed to load: " << file << std::endl;
			return;
		}

		sampleSize = fileD.tellg();
		sampleLength = sampleSize / sizeof(float);

		//std::cout << "Loaded Sample: " << file << " (" << sampleLength << " bytes)" << std::endl;

		sampleData = new float[sampleSize];
		fileD.seekg(fileD.beg);
		fileD.read((char*)sampleData, sampleSize);
		fileD.close();
		type = WAVE_SAMPLE;
	} */

	float tick()
	{
 		if (type == WAVE_SAMPLE || type == WAVE_SLICER)
		{
			if (sampleLength == 0)
				return 0;
		} 
		dspFrequency += (frequency - dspFrequency) * porta;
		dspVolume += (volume - dspVolume) * slew;

		phase += dspFrequency / sampleRate;
		phase = fmodf(phase, 1.);

		sampleCursor += (frequency / 261.63) * (22050.f / sampleRate);

		switch (type)
		{
		case WAVE_SAW:
			return (-.5f + phase) * volume * masterVolume;
			break;
		case WAVE_PLS:
			duty = .5f + (globalMod * .333f);
			return phase > duty ? volume * .5f * masterVolume : -volume * .5f * masterVolume;
			break;
		case WAVE_TRI:
			if (phase < .5f)
				return (-.5f + phase * 2.f) * volume * 2.f * masterVolume;
			else
				return (1.5f - phase * 2.f) * volume * 2.f * masterVolume;
			break;
		case WAVE_SIN:
			return sin(2.f * 3.14159f * phase) * volume * masterVolume;
			break;
		case WAVE_SAMPLE:
			return sampleData[(uint32_t)sampleCursor % sampleLength] * volume * 3.f * masterVolume;
			break;
		case WAVE_SLICER:
			return sampleData[(uint32_t)sampleCursor % sampleLength] * volume * 3.f * masterVolume;
			break;
		}

		return 0;
	}

	uint8_t noteOn;

	float masterVolume;

	float originalNote;
	float note;
	float bend;

	float phase;
	float frequency;
	float volume;

	float dspVolume;
	float dspFrequency;

	float porta;
	float channelPorta;
	float slew;
	float duty;
	uint8_t type;

	float sampleCursor;
	float* sampleData;
	uint32_t sampleSize;
	uint32_t sampleLength;
};

const int oscilatorsCount = 8;
Oscilator oscilators[oscilatorsCount];

float processOscillators()
{
	float mix = 0.f;
	for (int i = 0; i < oscilatorsCount; ++i)
	{
		mix += oscilators[i].tick() * .3;
	}

	return mix;
}

void handleMidi(uint32_t midi)
{
	uint8_t cmd;
	uint8_t channel;
	uint8_t note;
	uint8_t velocity;

	cmd = (midi & 0x000000FF);
	note = ((midi & 0x0000FF00) >> 8);
	velocity = ((midi & 0x00FF0000) >> 16);
	//printf("Midi MSG: %x %x %x\n", cmd, note, velocity);

	channel = cmd & 0x0F;
	cmd = cmd & 0xF0;

	if(channel >= oscilatorsCount)
		return;

	if (cmd == 0x90) // note on
	{
		//printf("1");
		oscilators[channel].note = note;
		//printf("2");
		oscilators[channel].volume = velocity / 127.f;
		//printf("3");
		oscilators[channel].frequency = noteToFreq(note + oscilators[channel].bend * 12.f);
		//printf("4");
		if (oscilators[channel].noteOn == 1) //legato, note was already on
		{
			oscilators[channel].porta = oscilators[channel].channelPorta;
		}
		else
		{
			oscilators[channel].originalNote = note;
		}
		//printf("5");
		oscilators[channel].noteOn = 1;
		//printf("6");
		oscilators[channel].sampleCursor = 0;
		//printf("7");
		if (oscilators[channel].type == WAVE_SLICER)
		{
			oscilators[channel].frequency = noteToFreq(60 + oscilators[channel].bend * 12.f);
			uint8_t slice = (note - 60);
			oscilators[channel].sampleCursor = slice * (oscilators[channel].sampleLength / 16.f);
		} 
		//std::cout << "Note On: " << std::dec << (int)note << " channel: " << (int)channel << " velocity: " << (int)velocity << std::endl;
	}
	else if (cmd == 0x80) // note off
	{
		if (oscilators[channel].originalNote == note)
		{
			oscilators[channel].volume = 0;
			oscilators[channel].noteOn = 0;
			oscilators[channel].porta = 1.f;
		}
		else
		{
			
			oscilators[channel].note = oscilators[channel].originalNote;

			if (oscilators[channel].type == WAVE_SLICER)
			{
				oscilators[channel].note = 60;
				oscilators[channel].frequency = noteToFreq(60 + oscilators[channel].bend * 12.f);
			}
			else
			{
				oscilators[channel].frequency = noteToFreq(oscilators[channel].note + oscilators[channel].bend * 12.f);
			}
		}
		//std::cout << "Note Off: " << std::dec << (int)note << " channel: " << (int)channel << " velocity: " << (int)velocity << std::endl;
	}
	else if (cmd == 0xE0) //pitch bend
	{
		uint16_t bend = (velocity << 7) | (note & 0x7f);
		float bendf = ((float)bend / 8191.5f) - 1.0f;

		oscilators[channel].bend = bendf;
		oscilators[channel].frequency = noteToFreq(oscilators[channel].note + bendf * 12.f);

		//std::cout << "pitch bend on channel: " << bendf << std::endl;
	}
	else if (cmd == 0xB0) // MIDI CC
	{
		switch (note) //cc num
		{
		case 7: //channel volume
			oscilators[channel].masterVolume = velocity / 101.6;
			break;
		case 5: //porta
			oscilators[channel].channelPorta = 1.0f / (1.f + velocity * 96.f);
			break;
		case 16: //type
			oscilators[channel].type = velocity / 22;
			break;
		}
		//std::cout << "MIDI CC: " << std::dec << (int)note << " channel: " << (int)channel << " value: " << (int)velocity << std::endl;
	}
}

const uint32_t bufferSize = 1024;
float buffer[bufferSize];
volatile uint32_t bufferCursor = 0;

volatile uint32_t irqCursor = 0;

bool repeating_timer_callback(struct repeating_timer *t)
{
	pwm_set_gpio_level(2, 2048.f + buffer[irqCursor] * 2048.f);
	irqCursor++;

	if(irqCursor == bufferSize)
	{
		irqCursor = 0;
		bufferCursor = 0;
	}

	return true;
}


int main()
{
    //set_sys_clock_khz(131000, true);

	vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(1);
	set_sys_clock_khz(400000, true);
    stdio_init_all();

	sleep_ms(500);

    printf("Starting in 500 ms\n");

	sleep_ms(500);

	printf("Started\n");

    const uint LED_PIN = 2;

    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(LED_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 4096);
    pwm_config_set_clkdiv(&config, 1.f);
    pwm_init(slice_num, &config, true);

	float tempo = 140.f;
	float quarterNoteMS = 60000.f / tempo;
	float ticksPerQuarterNote = 24;
	float tickMS = quarterNoteMS / ticksPerQuarterNote;
	float samplesPerTick = tickMS / (1000.f / sampleRate);

	int currentTick = 0;
	int eventIDX = 0;
	int sampleCounter = 0;

	uint32_t eventCount = sizeof(routineBin) / 6;
	printf("Event Count: %u\n", eventCount);

	oscilators[7].sampleData = (float*)thinkBreak;
	oscilators[7].sampleLength = sizeof(thinkBreak) / 4;
	oscilators[7].sampleSize = sizeof(thinkBreak);
	oscilators[7].type = WAVE_SLICER;

	struct repeating_timer timer;
	add_repeating_timer_us(-(1000000 / sampleRate), repeating_timer_callback, NULL, &timer);

	while (1)
	{
		if (sampleCounter % (int)samplesPerTick == 0)
		{
			if (eventIDX >= eventCount)
				break;

			for (int i = eventIDX; i < eventCount; ++i)
			{
				uint16_t delta = (routineBin[i * 6]) | (routineBin[i * 6 + 1] << 8);
				uint32_t midiD = (routineBin[i * 6 + 2]) | (routineBin[i * 6 + 3] << 8) | (routineBin[i * 6 + 4] << 16) | (routineBin[i * 6 + 5] << 24);
				if (delta <= currentTick)
				{
					eventIDX = i + 1;
					handleMidi(midiD);
				}
				else
				{
					break;
				}
			}
			currentTick++;
		}

		buffer[bufferCursor] = processOscillators();
		bufferCursor++;

		while(bufferCursor == bufferSize)
		{
			tight_loop_contents();
		}

		globalMod = sin(sampleCounter * 0.00001f);
		sampleCounter++;
	}
}