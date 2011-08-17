/*
Minetest audio system
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

#include <vorbis/vorbisfile.h>

#include "audio.h"

#include "filesys.h"

#include "debug.h"

#define BUFFER_SIZE 32768

static const char *alcErrorString(ALCenum err)
{
	switch (err) {
	case ALC_NO_ERROR:
		return "no error";
	case ALC_INVALID_DEVICE:
		return "invalid device";
	case ALC_INVALID_CONTEXT:
		return "invalid context";
	case ALC_INVALID_ENUM:
		return "invalid enum";
	case ALC_INVALID_VALUE:
		return "invalid value";
	case ALC_OUT_OF_MEMORY:
		return "out of memory";
	default:
		return "<unknown OpenAL error>";
	}
}

static const char *alErrorString(ALenum err)
{
	switch (err) {
	case AL_NO_ERROR:
		return "no error";
	case AL_INVALID_NAME:
		return "invalid name";
	case AL_INVALID_ENUM:
		return "invalid enum";
	case AL_INVALID_VALUE:
		return "invalid value";
	case AL_INVALID_OPERATION:
		return "invalid operation";
	case AL_OUT_OF_MEMORY:
		return "out of memory";
	default:
		return "<unknown OpenAL error>";
	}
}

/*
	Sound buffer
*/

core::map<std::string, SoundBuffer*> SoundBuffer::cache;

SoundBuffer* SoundBuffer::loadOggFile(const std::string &fname)
{
	// TODO if Vorbis extension is enabled, load the raw data

	int endian = 0;                         // 0 for Little-Endian, 1 for Big-Endian
	int bitStream;
	long bytes;
	char array[BUFFER_SIZE];                // Local fixed size array
	vorbis_info *pInfo;
	OggVorbis_File oggFile;

	if (cache.find(fname)) {
		dstream << "Ogg file " << fname << " loaded from cache"
			<< std::endl;
		return cache[fname];
	}

	// Try opening the given file
	if (ov_fopen(fname.c_str(), &oggFile) != 0)
	{
		dstream << "Error opening " << fname << " for decoding" << std::endl;
		return NULL;
	}

	SoundBuffer *snd = new SoundBuffer;

	// Get some information about the OGG file
	pInfo = ov_info(&oggFile, -1);

	// Check the number of channels... always use 16-bit samples
	if (pInfo->channels == 1)
		snd->format = AL_FORMAT_MONO16;
	else
		snd->format = AL_FORMAT_STEREO16;

	// The frequency of the sampling rate
	snd->freq = pInfo->rate;

	// Keep reading until all is read
	do
	{
		// Read up to a buffer's worth of decoded sound data
		bytes = ov_read(&oggFile, array, BUFFER_SIZE, endian, 2, 1, &bitStream);

		if (bytes < 0)
		{
			ov_clear(&oggFile);
			dstream << "Error decoding " << fname << std::endl;
			return NULL;
		}

		// Append to end of buffer
		snd->buffer.insert(snd->buffer.end(), array, array + bytes);
	} while (bytes > 0);

	alGenBuffers(1, &snd->bufferID);
	alBufferData(snd->bufferID, snd->format,
			&(snd->buffer[0]), snd->buffer.size(),
			snd->freq);

	ALenum error = alGetError();

	if (error != AL_NO_ERROR) {
		dstream << "OpenAL error: " << alErrorString(error)
			<< "preparing sound buffer"
			<< std::endl;
	}

	dstream << "Audio file " << fname << " loaded"
		<< std::endl;
	cache[fname] = snd;

	// Clean up!
	ov_clear(&oggFile);

	return cache[fname];
}

/*
	Sound sources
*/

SoundSource::SoundSource(const SoundBuffer *buf)
{
	m_buffer = buf;
	alGenSources(1, &sourceID);

	alSourcei(sourceID, AL_BUFFER, buf->getBufferID());
	alSourcei(sourceID, AL_SOURCE_RELATIVE,
			isRelative() ? AL_TRUE : AL_FALSE);

	alSource3f(sourceID, AL_POSITION, 0, 0, 0);
	alSource3f(sourceID, AL_VELOCITY, 0, 0, 0);
}

/*
	Audio system
*/

Audio *Audio::m_system = NULL;

Audio *Audio::system() {
	if (!m_system) {
		m_system = new Audio();
		if (!m_system)
			throw AudioSystemException("Failed to initialize audio system");
	}

	return m_system;
}

Audio::Audio() :
	m_device(NULL),
	m_context(NULL),
	m_can_vorbis(false)
{
	dstream << "Initializing audio system" << std::endl;

	ALCenum error = ALC_NO_ERROR;

	m_device = alcOpenDevice(NULL);
	if (!m_device) {
		dstream << "No audio device available, audio system not initialized"
			<< std::endl;
		return;
	}

	if (alcIsExtensionPresent(m_device, "EXT_vorbis")) {
		dstream << "Vorbis extension present, good" << std::endl;
		m_can_vorbis = true;
	} else {
		dstream << "Vorbis extension NOT present" << std::endl;
		m_can_vorbis = false;
	}

	m_context = alcCreateContext(m_device, NULL);
	if (!m_context) {
		error = alcGetError(m_device);
		dstream << "Unable to initialize audio context, aborting audio initialization"
			<< " (" << alcErrorString(error) << ")"
			<< std::endl;
		alcCloseDevice(m_device);
		m_device = NULL;
	}

	if (!alcMakeContextCurrent(m_context) ||
			(error = alcGetError(m_device) != ALC_NO_ERROR))
	{
		dstream << "Error setting audio context, aborting audio initialization"
			<< " (" << alcErrorString(error) << ")"
			<< std::endl;
		shutdown();
	}

	dstream << "Audio system initialized: OpenAL "
		<< alGetString(AL_VERSION)
		<< ", using " << alcGetString(m_device, ALC_DEVICE_SPECIFIER)
		<< std::endl;

}

Audio::~Audio()
{
	if (!isAvailable())
		return;

	shutdown();
}

void Audio::shutdown()
{
	alcMakeContextCurrent(NULL);
	alcDestroyContext(m_context);
	m_context = NULL;
	alcCloseDevice(m_device);
	m_device = NULL;

	dstream << "OpenAL context and devices cleared" << std::endl;
}

void Audio::init(const std::string &path)
{
	if (fs::PathExists(path)) {
		m_path = path;
		dstream << "Audio: using sound path " << path
			<< std::endl;
	} else {
		dstream << "WARNING: audio path " << path
			<< " not found, sounds will not be available."
			<< std::endl;
	}
}

enum LoaderFormat {
	LOADER_VORBIS,
	LOADER_WAV,
	LOADER_UNK,
};

static const char *extensions[] = {
	"ogg", "wav", NULL
};

std::string Audio::findSoundFile(const std::string &basename, u8 &fmt)
{

	std::string base(m_path + basename + ".");

	fmt = LOADER_VORBIS;
	const char **ext = extensions;

	while (*ext) {
		std::string candidate(base + *ext);
		if (fs::PathExists(candidate))
			return candidate;
		++ext;
		++fmt;
	}

	return "";
}

AmbientSound *Audio::getAmbientSound(const std::string &basename)
{
	if (!isAvailable())
		return NULL;

	AmbientSoundMap::Node* cached = m_ambient_sound.find(basename);

	if (cached)
		return cached->getValue();

	SoundBuffer *data(loadSound(basename));
	if (!data) {
		dstream << "Ambient sound "
			<< " '" << basename << "' not available"
			<< std::endl;
		return NULL;
	}

	AmbientSound *snd = new AmbientSound(data);
	if (snd) {
		m_ambient_sound[basename] = snd;
	}

	return snd;
}

void Audio::setAmbient(const std::string &slotname,
		const std::string &basename)
{
	if (!isAvailable())
		return;

	if (m_ambient_slot.find(slotname))
		((AmbientSound*)(m_ambient_slot[slotname]))->stop();

	if (basename.empty()) {
		m_ambient_slot.remove(slotname);
		return;
	}

	AmbientSound *snd = getAmbientSound(basename);

	if (snd) {
		m_ambient_slot[slotname] = snd;
		snd->play();
		dstream << "Ambient " << slotname
			<< " switched to " << basename
			<< std::endl;
	} else {
		m_ambient_slot.remove(slotname);
		dstream << "Ambient " << slotname
			<< " could not switch to " << basename
			<< ", cleared"
			<< std::endl;
	}

}

SoundBuffer* Audio::loadSound(const std::string &basename)
{
	if (!isAvailable())
		return NULL;

	u8 fmt;
	std::string fname(findSoundFile(basename, fmt));

	if (fname.empty()) {
		dstream << "WARNING: couldn't find audio file "
			<< basename << " in " << m_path
			<< std::endl;
		return NULL;
	}

	dstream << "Audio file '" << basename
		<< "' found as " << fname
		<< std::endl;

	switch (fmt) {
	case LOADER_VORBIS:
		return SoundBuffer::loadOggFile(fname);
	}

	dstream << "WARNING: no appropriate loader found "
		<< " for audio file " << fname
		<< std::endl;

	return NULL;
}

