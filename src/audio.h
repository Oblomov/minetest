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

public:
	ALuint getBufferID() const
		{ return bufferID; };

private:
	ALenum	format;
	ALsizei	freq;
	ALuint	bufferID;

	std::vector<char> buffer;
};

class SoundSource
{
public:
	SoundSource(const SoundBuffer *buf);
	virtual bool isRelative() const { return false; }
	virtual void stop() const
	{
		alSourceStop(sourceID);
	}
	virtual void play() const
	{
		alSourcePlay(sourceID);
	}
	virtual void loop(bool setting=true)
	{
		alSourcei(sourceID, AL_LOOPING, setting ? AL_TRUE : AL_FALSE);
	}
protected:
	ALuint	sourceID;

	const SoundBuffer* m_buffer;
};

class AmbientSound : public SoundSource
{
public:
	AmbientSound(SoundBuffer *buf=NULL) : SoundSource(buf)
	{
		loop();
		// no rolloff
		alSourcei(sourceID, AL_ROLLOFF_FACTOR, 0);
	}

	virtual bool isRelative() const { return true; }
};

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

	void setAmbient(const std::string &slotname,
			const std::string &basename);

private:
	Audio();
	~Audio();

	void shutdown();

	std::string findSoundFile(const std::string &basename, u8 &fmt);
	SoundBuffer* loadSound(const std::string &basename);

	std::string m_path;
	ALCdevice *m_device;
	ALCcontext *m_context;

	AmbientSound *getAmbientSound(const std::string &basename);

	typedef core::map<std::string, AmbientSound *> AmbientSoundMap;
	// map slot to currently assigned ambient sound to that
	// slot
	AmbientSoundMap m_ambient_slot;
	// map ambient sound name to actual ambient sound
	AmbientSoundMap m_ambient_sound;

	bool m_can_vorbis;
};

#endif // SERVER

#endif //AUDIO_HEADER

