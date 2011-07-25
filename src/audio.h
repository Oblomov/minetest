/*
 *Minetest-delta
 *Copyright (C) 2011 Sebastian Rühl <https://launchpad.net/~sebastian-ruehl>
 *
 *This program is free software: you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation, either version 2 of the License, or
 *(at your option) any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AUDIO_H_
#define AUDIO_H_

//ONLY for the client audio is relevant
#ifndef SERVER

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <AL/alut.h>

#include <vorbis/vorbisfile.h>

#include <debug.h>
#include "utility.h"
#include "common_irrlicht.h"
#include <map>
#include <vector>


#define BUFFER_SIZE   32768 // For OGG buffer


// Must be implemented by Object in the world
class AudioSource
{
public:
	// The position of Sound in the World
	virtual v3f getPosition()
	{
		return v3f(0,0,0);
	}

	// The veloicty of Sound in the World
	virtual v3f getVelocity()
	{
		return v3f(0,0,0);
	}

	// The soundfile
	virtual const char* getAudioFile() = 0;

	// Returns true if the sound should be played
	//virtual bool isActive() = 0;
	virtual bool isActive()
	{
		return active;
	}

	// Return the reference distance
	// Means until want distance its possible to hear the sound
	// Usefull is something between 30.0 and 100.0
	virtual float getReferenceDistance()
	{
		return 30.0;
	}

	virtual bool isLooping()
	{
		return true;
	}

	virtual bool isRelative()
	{
		return false;
	}

	virtual float getPitch()
	{
		return 1.0f;
	}

	virtual float getGain()
	{
		return 1.0f;
	}
	
	bool active;
};


class FootstepAudioSource: public AudioSource
{
	const char* getAudioFile()
	{
		return "data/sounds/footstep.wav";
	}
	
	virtual bool isRelative()
	{
		return true;
	}
};

class JumpAudioSource: public AudioSource
{
	const char* getAudioFile()
	{ 
		return "data/sounds/jump.wav";
	}
	
	virtual bool isRelative()
	{
		return true;
	}
};

struct AudioSourceData
{
	ALuint Buffer;
	ALuint Source;
	ALfloat Pitch;
	ALfloat Gain;
	ALfloat SourcesPos[3];
	ALfloat SourcesVel[3];
	ALfloat ReferenceDistance;
	ALboolean Looping;
	ALboolean Relative;
	
	ALenum Format;
	ALsizei Freq;
	
	std::vector<char> BufferData;
};

enum AudioState
{
	UNITALIZED, INITALIZED, ENDED, ERROR
};


class Audio
{
public:
	Audio();
	~Audio();

	// Initalize the AudioSubsytem
	bool initalize();
	
	// Load a OGG file
	void loadOggFile(const char *fileName, std::vector<char> &buffer, ALenum &format, ALsizei &freq);
	
	// Register a sound source within the sound system
	void registerSoundSource(AudioSource* source);

	// Remove a sound source form the sound system
	void removeSoundSource(AudioSource* source);

	// Set a ambient sound
	void setAmbientSound(const char*);
	
	// Processes the Sounds
	void processSound();

	// Sets Player position
	void updatePlayerPostion(v3f playerPosition);

	// Sets the camera for obtaining the orientation
	// Note: should only set once.
	void setPlayerCamera(scene::ICameraSceneNode* camera);

	// Update the Player Orientation
	void updateOrientation();

private:

	/*
	 * Load data into the sound system.
	 */
	AudioSourceData LoadAlData(AudioSource* audioSource);

	/*
	 * Update the listenerValues
	 */
	void updateListenerValues();

	void KillALData();

	/*
	 * Displays errorMessage and
	 * returns true if there was an Error
	 */
	bool handleAlutError(std::string errorMessage);

	void updateAudioSourceData(AudioSource* audioSource, AudioSourceData& audioSourceData);

	void updateAlValues(AudioSourceData& audioSourceData);

	/*
	 * The AudioSource registered withing the system
	 */
	std::map<AudioSource*, AudioSourceData> m_audioSources;

	/*
	 * The Cached audio buffers
	 * maps a filename to a buffer
	 */
	std::map<const char*, ALuint> m_audioBuffers;

	scene::ICameraSceneNode* m_camera;

	AudioState m_state;

	ALCdevice* m_device;

	ALCcontext * m_context;

	////
	// The Listener of the sounds
	////

	// Position of the listener.
	ALfloat m_ListenerPos[3];

	// Velocity of the listener.
	ALfloat m_ListenerVel[3];

	// Orientation of the listener. (first 3 elements are "at", second 3 are "up")
	ALfloat m_ListenerOri[6];

	//
	////

};

#endif

#endif /* AUDIO_H_ */
