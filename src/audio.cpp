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
#include "camera.h"

#include "filesys.h"

#include "debug.h"

using std::nothrow;

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

// check if audio source is actually present
// (presently, if its buf is non-zero)
// see also define in audio.h
#define _SOURCE_CHECK if (m_buffer == NULL) return

SoundSource::SoundSource(const SoundBuffer *buf) :
	m_relative(false)
{
	m_buffer = buf;

	_SOURCE_CHECK;

	alGenSources(1, &sourceID);

	alSourcei(sourceID, AL_BUFFER, buf->getBufferID());

	alSource3f(sourceID, AL_POSITION, 0, 0, 0);
	alSource3f(sourceID, AL_VELOCITY, 0, 0, 0);

	alSourcef(sourceID, AL_ROLLOFF_FACTOR, 0.7);
}

#undef _SOURCE_CHECK

/*
	Audio system
*/

Audio *Audio::m_system = NULL;

Audio *Audio::system() {
	if (!m_system)
		m_system = new Audio();

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

	alDistanceModel(AL_EXPONENT_DISTANCE);

	dstream << "Audio system initialized: OpenAL "
		<< alGetString(AL_VERSION)
		<< ", using " << alcGetString(m_device, ALC_DEVICE_SPECIFIER)
		<< std::endl;
}


// check if audio is available, returning if not
#define _CHECK_AVAIL if (!isAvailable()) return

Audio::~Audio()
{
	_CHECK_AVAIL;

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
	// prepare an empty ambient sound that is to be used when
	// mapped sounds are not present
	m_ambient_sound[""] = new AmbientSound(NULL);
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
	_CHECK_AVAIL NULL;

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

	AmbientSound *snd(new (nothrow) AmbientSound(data));
	if (snd)
		m_ambient_sound[basename] = snd;
	return snd;
}

void Audio::setAmbient(const std::string &slotname,
		const std::string &basename, bool autoplay)
{
	_CHECK_AVAIL;

	bool was_playing = autoplay;

	AmbientSound *snd = getAmbientSound(basename);

	if (m_ambient_slot.find(slotname)) {
		AmbientSound *oldsnd = m_ambient_slot[slotname];
		if (oldsnd == snd)
			return;
		was_playing = oldsnd->isPlaying();
		if (was_playing)
			oldsnd->stop();
	}

	if (snd) {
		if (was_playing || autoplay)
			snd->play();
		m_ambient_slot[slotname] = snd;
		dstream << "Ambient " << slotname
			<< " switched to " << basename
			<< std::endl;
	} else {
		// FIXME two-step assignment to cope with irrMap limitations
		snd = m_ambient_sound[""];
		m_ambient_slot[slotname] = snd;
		dstream << "Ambient " << slotname
			<< " could not switch to " << basename
			<< ", cleared"
			<< std::endl;
	}
}

SoundSource *Audio::createSource(const std::string &sourcename,
		const std::string &basename)
{
	SoundSourceMap::Node* present = m_sound_source.find(sourcename);

	if (present) {
		dstream << "WARNING: attempt to re-create sound source "
			<< sourcename << std::endl;
		return present->getValue();
	}

	SoundBuffer *data(loadSound(basename));
	if (!data) {
		dstream << "Sound source " << sourcename << " not available: "
			<< basename << " could not be loaded"
			<< std::endl;
	}

	SoundSource *snd = new (nothrow) SoundSource(data);
	m_sound_source[sourcename] = snd;

	return snd;
}

SoundSource *Audio::getSource(const std::string &sourcename)
{
	SoundSourceMap::Node* present = m_sound_source.find(sourcename);

	if (present)
		return present->getValue();

	dstream << "WARNING: attempt to get sound source " << sourcename
		<< " before it was created! Creating an empty one"
		<< std::endl;

	SoundSource *snd = new (nothrow) SoundSource(NULL);
	m_sound_source[sourcename] = snd;

	return snd;
}

void Audio::updateListener(const scene::ICameraSceneNode* cam, const v3f &vel)
{
	_CHECK_AVAIL;

	v3f pos = cam->getPosition();
	m_listener[0] = pos.X;
	m_listener[1] = pos.Y;
	m_listener[2] = pos.Z;

	m_listener[3] = vel.X;
	m_listener[4] = vel.Y;
	m_listener[5] = vel.Z;

	v3f at = cam->getTarget();
	m_listener[6] = (pos.X - at.X);
	m_listener[7] = (pos.Y - at.Y);
	m_listener[8] = (at.Z - pos.Z); // oh yeah
	v3f up = cam->getUpVector();
	m_listener[9] = up.X;
	m_listener[10] = up.Y;
	m_listener[11] = up.Z;

	alListenerfv(AL_POSITION, m_listener);
	alListenerfv(AL_VELOCITY, m_listener + 3);
	alListenerfv(AL_ORIENTATION, m_listener + 6);
}

SoundBuffer* Audio::loadSound(const std::string &basename)
{
	_CHECK_AVAIL NULL;

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

#undef _CHECK_AVAIL

