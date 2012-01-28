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

#include <cstdlib> // rand

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

// TODO this should be done after checking that no sound source uses the buffer
SoundBuffer::~SoundBuffer()
{
	alDeleteBuffers(1, &bufferID);
}

/*
	Sound sources
*/

// check if audio source is actually present
// (presently, if its buf is non-zero)
// see also define in audio.h
#define _SOURCE_CHECK if (m_buffer.empty()) return

SoundSource::SoundSource(const SoundBuffer *buf) :
	m_relative(false)
{
	if (buf)
		m_buffer.push_back(buf);

	alGenSources(1, &sourceID);

	alSource3f(sourceID, AL_POSITION, 0, 0, 0);
	alSource3f(sourceID, AL_VELOCITY, 0, 0, 0);

	alSourcef(sourceID, AL_ROLLOFF_FACTOR, 0.7);

	_SOURCE_CHECK;

	alSourcei(sourceID, AL_BUFFER, buf->getBufferID());
}

SoundSource::~SoundSource()
{
	alDeleteSources(1, &sourceID);
}

void
SoundSource::addAlternative(const SoundBuffer *buf)
{
	if (buf) {
		m_buffer.push_back(buf);
		if (m_buffer.size() == 1)
			alSourcei(sourceID, AL_BUFFER, buf->getBufferID());
	}
}

void
SoundSource::replace(const SoundSource *src)
{
	bool playing = isPlaying();
	if (playing)
		stop(); // TODO double buffer
	m_buffer = src->m_buffer;
	if (m_buffer.size() > 0)
		alSourcei(sourceID, AL_BUFFER, m_buffer[0]->getBufferID());
	else
		alSourcei(sourceID, AL_BUFFER, 0);
	if (playing)
		play(); // TODO double buffer
}

void
SoundSource::play() const
{
	_SOURCE_CHECK;
	/* if we have multiple sound buffers, pick a random
	   one to play. This must be done with the source
	   in the stopped state. The behavior is not inconsistent
	   with the single-source case because even in that case
	   alSourcePlay() would stop and restart the execution. */
	if (m_buffer.size() > 1) {
		alSourceStop(sourceID);
		size_t rnd = rand() % m_buffer.size();
		alSourcei(sourceID, AL_BUFFER, m_buffer[rnd]->getBufferID());
	}
	alSourcePlay(sourceID);
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
	// empty sound sources to be used when mapped sounds are not present
	m_sound_source[""] = new AmbientSound(NULL);
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

const SoundSource *Audio::getSoundSource(const std::string &basename)
{
	_CHECK_AVAIL NULL;

	const SoundSourceCache::iterator cached = m_sound_source.find(basename);

	if (cached != m_sound_source.end())
		return cached->second;

	const SoundSource *snd(loadSound(basename));

	if (!snd) {
		dstream << "Sound '" << basename << "' not available"
			<< std::endl;
		snd = new SoundSource(NULL);
	}
	m_sound_source[basename] = snd;
	return snd;
}

void Audio::setPlayerSound(const std::string &slotname,
		const std::string &basename)
{
	_CHECK_AVAIL;

	PlayerSound *slot;

	if (m_player_slot.count(slotname) == 0)
		m_player_slot[slotname] = new PlayerSound(NULL);

	slot = m_player_slot[slotname];

	if (slot->currentMap().compare(basename) == 0)
		return;

	const SoundSource *snd = getSoundSource(basename);

	slot->replace(snd);
	slot->mapTo(basename);
	dstream << "Player sound " << slotname
		<< " switched to " << basename
		<< std::endl;
}

void Audio::setAmbient(const std::string &slotname,
		const std::string &basename, bool autoplay)
{
	_CHECK_AVAIL;

	AmbientSound *slot;

	if (m_ambient_slot.count(slotname) == 0)
		m_ambient_slot[slotname] = new AmbientSound(NULL);

	slot = m_ambient_slot[slotname];

	if (slot->currentMap().compare(basename) == 0)
		return;

	const SoundSource *snd = getSoundSource(basename);

	slot->replace(snd);
	slot->mapTo(basename);
	if (autoplay)
		slot->play();

	dstream << "Ambient " << slotname
		<< " switched to " << basename
		<< std::endl;
}

SoundSource *Audio::createSource(const std::string &sourcename,
		const std::string &basename)
{
	SoundSource *slot;

	SoundSourceMap::iterator present = m_sound_slot.find(sourcename);

	if (present != m_sound_slot.end()) {
		dstream << "WARNING: attempt to re-create sound source "
			<< sourcename << std::endl;
		slot = present->second;
	} else {
		m_sound_slot[sourcename] = slot = new SoundSource(NULL);
	}

	const SoundSource *snd = getSoundSource(basename.empty() ?
			sourcename : basename);

	slot->replace(snd);
	slot->mapTo(basename);
	dstream << "Created sound source " << sourcename
		<< " with sound " << basename
		<< std::endl;

	return slot;
}

SoundSource *Audio::getSource(const std::string &sourcename)
{
	SoundSourceMap::iterator present = m_sound_slot.find(sourcename);

	if (present != m_sound_slot.end())
		return present->second;

	dstream << "WARNING: attempt to get sound source " << sourcename
		<< " before it was created! Creating an empty one"
		<< std::endl;

	return createSource(sourcename);
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

/* list of suffixes to the base name to be used to look for alternatives to a
 * sound buffer; for the moment, it's only the 1-9 numerics, so it could actually
 * have been done smarter */
static const char* altsfx[] = {
	"1", "2", "3",
	"4", "5", "6",
	"7", "8", "9",
	NULL
};

SoundSource *
Audio::loadSound(const std::string &basename)
{
	_CHECK_AVAIL NULL;

	SoundBuffer *buf = NULL;
	SoundSource* source = NULL;
	u8 fmt;
	std::vector<std::string> alts;
	std::vector<u8> fmts;

	std::string fname(findSoundFile(basename, fmt));

	if (fname.empty()) {
		// look for alternatives <basename><sfx>
		const char **sfx = altsfx;
		while (*sfx) {
			fname = findSoundFile(basename + *sfx, fmt);
			if (!fname.empty()) {
				alts.push_back(fname);
				fmts.push_back(fmt);
			}
			++sfx;
		}
	} else {
		// basename found, use it
		alts.push_back(fname);
		fmts.push_back(fmt);
	}

	if (alts.empty()) {
		dstream << "WARNING: couldn't find audio file "
			<< basename << " in " << m_path
			<< std::endl;
		return source;
	}

	dstream << "Audio file '" << basename
		<< "' found as ";
	for (size_t i = 0; i < alts.size() - 1; ++i)
		dstream << alts[i] << ", ";
	dstream << alts[alts.size() - 1] << std::endl;

	source = new (nothrow) SoundSource(NULL);
	if (!source) {
		dstream << "WARNING: failed to allocate memory for a new sound source!"
			<< std::endl;
		return source;
	}

	for (size_t i = 0; i < alts.size(); ++i) {
		fname = alts[i];
		fmt = fmts[i];
		buf = NULL;

		switch (fmt) {
		case LOADER_VORBIS:
			buf = SoundBuffer::loadOggFile(fname);
		}

		if (buf)
			source->addAlternative(buf);
		else
			dstream << "WARNING: no appropriate loader found "
				<< " for audio file " << fname
				<< std::endl;
	}

	return source;
}

#undef _CHECK_AVAIL

