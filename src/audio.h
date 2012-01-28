/*
Minetest audio system
Copyright (C) 2011 Sebastian 'Bahamada' Rühl
Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

Part of the minetest project
Copyright (C) 2010-2011 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef AUDIO_HEADER
#define AUDIO_HEADER

// Audio is only relevant for client
#ifndef SERVER

// gotta love CONSISTENCY
#if defined(_MSC_VER)
#include <al.h>
#include <alc.h>
#include <alext.h>
#elif defined(__APPLE__)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <OpenAL/alext.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

#include <string>
#include <vector>
#include <map>

#include "common_irrlicht.h"
#include "exceptions.h"

class AudioSystemException : public BaseException
{
public:
	AudioSystemException(const char *s):
		BaseException(s)
	{}
};

/* sound data + cache */
class SoundBuffer
{
public:
	static SoundBuffer* loadOggFile(const std::string &fname);
private:
	static core::map<std::string, SoundBuffer*> cache;
	~SoundBuffer(); // prevent deletion

public:
	ALuint getBufferID() const
		{ return bufferID; };

private:
	ALenum	format;
	ALsizei	freq;
	ALuint	bufferID;

	std::vector<char> buffer;
};

// check if audio source is actually present
// (presently, if its buffer is non-zero)
// TODO some kind of debug message

#define _SOURCE_CHECK if (m_buffer.empty()) return

class SoundSource
{
public:
	/* create sound source attached to sound buffer */
	SoundSource(const SoundBuffer *buf);
	~SoundSource();

	virtual void addAlternative(const SoundBuffer *buf);
	virtual size_t countAlternatives() const {
		return m_buffer.size();
	}

	/* replace our sound buffers with the ones from src */
	virtual void replace(const SoundSource *src);

	virtual void setRelative(bool rel=true)
	{
		m_relative = rel;
		alSourcei(sourceID, AL_SOURCE_RELATIVE,
				rel ? AL_TRUE : AL_FALSE);
	}
	virtual bool isRelative() const { return m_relative; }

	virtual void stop() const
	{
		_SOURCE_CHECK;
		alSourceStop(sourceID);
	}

	virtual bool isPlaying() const
	{
		_SOURCE_CHECK false;
		ALint val;
		alGetSourcei(sourceID, AL_SOURCE_STATE, &val);
		return val == AL_PLAYING;
	}

	// play sound, picking a random buffer if we have more than one
	virtual void play() const;

	/* should a sound be playing or not?
	   while in the false case it's the same as calling stop(),
	   for the true case its effect is different from calling
	   play() directly because play() causes the sound to restart
	   and it can possibly pick a different sample for multi-sample
	   sound sources */
	virtual void shouldPlay(bool should=true)
	{
		_SOURCE_CHECK;
		bool playing = isPlaying();
		if (should && !playing)
			play();
		else if (!should && playing)
			stop();
		// otherwise do nothing
	}

	virtual void loop(bool setting=true)
	{
		_SOURCE_CHECK;
		alSourcei(sourceID, AL_LOOPING, setting ? AL_TRUE : AL_FALSE);
	}

	virtual v3f getPosition() const
	{
		_SOURCE_CHECK v3f(0,0,0);
		v3f pos;
		alGetSource3f(sourceID, AL_POSITION,
				&pos.X, &pos.Y, &pos.Z);
		return pos;
	}

	virtual void setPosition(const v3f& pos)
	{
		_SOURCE_CHECK;
		alSource3f(sourceID, AL_POSITION, pos.X, pos.Y, pos.Z);
	}
	virtual void setPosition(ALfloat x, ALfloat y, ALfloat z)
	{
		_SOURCE_CHECK;
		alSource3f(sourceID, AL_POSITION, x, y, z);
	}

	virtual void setReferenceDistance(float dist)
	{
		_SOURCE_CHECK;
		alSourcef(sourceID, AL_REFERENCE_DISTANCE, dist);
	}

	virtual void mapTo(const std::string &text)
	{ m_map = text; }
	virtual const std::string& currentMap() const
	{ return m_map; }

protected:
	ALuint	sourceID;

	std::vector<const SoundBuffer*> m_buffer;
	std::string m_map;
	bool m_relative;

private: /* sound sources should not be copied around */
	SoundSource(const SoundSource &org);
	SoundSource& operator=(const SoundSource &org);
};

/* ambient sounds are atmospheric sounds like background music
   and unpositioned environmental noise (looping) */

class AmbientSound : public SoundSource
{
public:
	AmbientSound(SoundBuffer *buf=NULL) : SoundSource(buf)
	{
		_SOURCE_CHECK;
		loop();
		setRelative();
	}
};

/* player sounds are sounds emitted by the player or otherwise
   relative to the player position (e.g. HUD) */

class PlayerSound : public SoundSource
{
public:
	PlayerSound(SoundBuffer *buf=NULL) : SoundSource(buf)
	{
		_SOURCE_CHECK;
		setRelative();
	}
};

#undef _SOURCE_CHECK

class Audio
{
	/* static interface */
public:
	static Audio *system();
private:
	static Audio *m_system;

	/* audio system interface */
public:
	/* (re)initialize the sound/music file path */
	void init(const std::string &path);
	bool isAvailable() const { return m_context != NULL; }

	/* assign a specific ambient sound to the given ambient slot */
	void setAmbient(const std::string &slotname,
			const std::string &basename,
			bool autoplay=true);

	/* assign a specific player sound to the given player slot */
	void setPlayerSound(const std::string &slotname,
			const std::string &basename);

	/* get a specific ambient/player sound */
	AmbientSound *ambientSound(const std::string &slotname)
	{ return m_ambient_slot[slotname];}
	PlayerSound *playerSound(const std::string &slotname)
	{ return m_player_slot[slotname];}

	void updateListener(const scene::ICameraSceneNode* cam, const v3f &vel);

	SoundSource *createSource(const std::string &sourcename,
			const std::string &basename="");
	SoundSource *getSource(const std::string &sourcename);

private:
	Audio();
	~Audio();

	void shutdown();

	std::string findSoundFile(const std::string &basename, u8 &fmt);
	SoundSource *loadSound(const std::string &basename);

	std::string m_path;
	ALCdevice *m_device;
	ALCcontext *m_context;

	const SoundSource *getSoundSource(const std::string &basename);

	typedef std::map<std::string, const SoundSource *> SoundSourceCache;
	typedef std::map<std::string, AmbientSound *> AmbientSoundMap;
	typedef std::map<std::string, PlayerSound *> PlayerSoundMap;
	typedef std::map<std::string, SoundSource *> SoundSourceMap;

	// sound source cache
	SoundSourceCache m_sound_source;
	// map slot name to ambient sound source
	AmbientSoundMap m_ambient_slot;
	// map slot name to player sound source
	PlayerSoundMap m_player_slot;
	// map generic sound name to sound source
	SoundSourceMap m_sound_slot;

	bool m_can_vorbis;

	// listener position/velocity/orientation
	ALfloat m_listener[12];

};

#endif // SERVER

#endif //AUDIO_HEADER

