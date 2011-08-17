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

#include <vorbis/vorbisfile.h>
#include "audio.h"


Audio::Audio()
{
	m_state = UNITALIZED;
	for (int i = 0; i<3; i++)
	{
		m_ListenerPos[i] = 0;
		m_ListenerVel[i] = 0;
		m_ListenerOri[i] = 0;
		m_ListenerOri[i+3] = 0;
	}
}

Audio::~Audio()
{
	KillALData();
}

bool Audio::initalize()
{
	// Initialize OpenAL and clear the error bit.

	// Initialization
	m_device = alcOpenDevice(NULL); // select the "preferred device"

	if (m_device)
	{
		m_context=alcCreateContext(m_device,NULL);
		alcMakeContextCurrent(m_context);
	}

	alutInit(NULL, 0);
	alGetError(); //clear error code

	m_state = INITALIZED;
	dstream << "Audio initalized" << std::endl;
	return true;
}

void Audio::loadOggFile(const char *fileName, std::vector<char> &buffer, ALenum &format, ALsizei &freq)
{
    int endian = 0;                         // 0 for Little-Endian, 1 for Big-Endian
    int bitStream;
    long bytes;
    char array[BUFFER_SIZE];                // Local fixed size array
    vorbis_info *pInfo;
    OggVorbis_File oggFile;
    

    // Try opening the given file
    if (ov_fopen(fileName, &oggFile) != 0)
    {
    	dstream << "Error opening " << fileName << " for decoding..." << std::endl;
    	return;
    }
    
     // Get some information about the OGG file
    pInfo = ov_info(&oggFile, -1);

    // Check the number of channels... always use 16-bit samples
    if (pInfo->channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;

    // The frequency of the sampling rate
    freq = pInfo->rate;

    // Keep reading until all is read
    do
    {
        // Read up to a buffer's worth of decoded sound data
        bytes = ov_read(&oggFile, array, BUFFER_SIZE, endian, 2, 1, &bitStream);

        if (bytes < 0)
        {
            ov_clear(&oggFile);
            dstream << "Error decoding " << fileName << "..." << std::endl;
            return;
        }

        // Append to end of buffer
        buffer.insert(buffer.end(), array, array + bytes);
   	} while (bytes > 0);

    // Clean up!
    ov_clear(&oggFile);
}

void Audio::registerSoundSource(AudioSource* source)
{
	if (m_audioSources.find(source) == m_audioSources.end())
	{
		m_audioSources[source] = LoadAlData(source);
	}
}

void Audio::removeSoundSource(AudioSource* source)
{
	m_audioSources.erase(source);
}


void Audio::setAmbientSound(const char* ambientSoundFile)
{
	/*
	 * TODO: Two instances overlap. Need to fix that.
	 */
	 
	/*ALuint buffer = alutCreateBufferFromFile(ambientSoundFile);
	if(handleAlutError("Error creating buffer for Ambient Sound"))
		return;
	ALuint source;
	alGenSources(1, &source);
	alSourcei (source, AL_BUFFER,   buffer   );
	alSourcef (source, AL_PITCH,    1.0f     );
	alSourcef(source, AL_GAIN,     1.0f     );
	alSourcei(source, AL_LOOPING, true);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	handleAlutError("Error while setting attributes for ambient Sound");
	alSourcePlay(source);
	handleAlutError("Error on Handling Ambient playback Sound");*/
	
	//AudioSource * data = new AudioSource();
	AudioSourceData data;
	alGenBuffers(1, &data.Buffer);
	alGenSources(1, &data.Source);
	alListener3f(AL_POSITION, data.SourcesPos[0], data.SourcesPos[1], data.SourcesPos[2]);
    alSource3f(data.Source, AL_POSITION, data.SourcesPos[0], data.SourcesPos[1], data.SourcesPos[2]);
    
    loadOggFile(ambientSoundFile, data.BufferData, data.Format, data.Freq);
    
    alBufferData(data.Buffer, data.Format, &data.BufferData[0], static_cast<ALsizei>(data.BufferData.size()), data.Freq);
	alSourcei(data.Source, AL_BUFFER, data.Buffer);
	alSourcePlay(data.Source);
	
	//alDeleteBuffers(1, &data.Buffer);
  	//alDeleteSources(1, &data.Source);
}

void Audio::processSound()
{
	ALint play;
	for(
	    std::map<AudioSource*, AudioSourceData>::iterator it = m_audioSources.begin();
	    it != m_audioSources.end();
	    ++it
	)
	{
		AudioSource* source = (*it).first;
		AudioSourceData data = (*it).second;

		updateAudioSourceData(source, data);
		updateAlValues(data);

		alGetSourcei(data.Source,AL_SOURCE_STATE, &play);
		if (play == AL_PLAYING)
		{
			// In case the source isn't anymore active
			// Stop the sound
			if(!(source->isActive()))
			{
				dstream << "Source is not active anymore. Stopping Playback"
				        << data.Source << std::endl;
				alSourceStop(data.Source);
			}
			continue;
		}
		else
		{
			if (source->isActive())
			{
				dstream << "Source is active so we start playback:" << data.Source <<std::endl;
				alSourcePlay(data.Source);
				handleAlutError("Playback Error");
			}
		}
	}
}

void Audio::updatePlayerPostion(v3f playerPosition)
{
	m_ListenerPos[0] = playerPosition.X;
	m_ListenerPos[1] = playerPosition.Y;
	m_ListenerPos[2] = playerPosition.Z;

	alListenerfv(AL_POSITION,    m_ListenerPos);
}

void Audio::setPlayerCamera(scene::ICameraSceneNode* camera)
{
	m_camera = camera;
}

void Audio::updateOrientation()
{
	v3f at = m_camera->getTarget();
	v3f up = m_camera->getUpVector();
	m_ListenerOri[0] = at.X;
	m_ListenerOri[1] = at.Y;
	m_ListenerOri[2] = at.Z;

	m_ListenerOri[3] = up.X;
	m_ListenerOri[4] = up.Y;
	m_ListenerOri[5] = up.Z;

	alListenerfv(AL_VELOCITY,    m_ListenerVel);
	alListenerfv(AL_ORIENTATION, m_ListenerOri);
}

//TODO: we need an exception
AudioSourceData Audio::LoadAlData(AudioSource* audioSource)
{
	AudioSourceData data;
	updateAudioSourceData(audioSource, data);

	dstream << "Trying to create buffer" << std::endl;

	const char * audioFile = audioSource->getAudioFile();
	// Look if the file is allready cached
	std::map<const char *,ALuint>::iterator it = m_audioBuffers.find(audioFile);
	ALuint Buffer;
	if (it == m_audioBuffers.end())
	{
		// We need to load the buffer form a file
		Buffer = alutCreateBufferFromFile(audioFile);
		// TODO: manage buffer size
		m_audioBuffers[audioFile] = Buffer;
	}
	else
	{
		// Our iterator points to the Buffer
		Buffer = (*it).second;
	}

	data.Buffer = Buffer;
	if (handleAlutError("Error creating Buffer"))
	{
		//TODO: throw exception
		return data;
	}

	dstream << "Trying to load audio to buffer" << std::endl;

	// Bind buffer with a source.
	alGenSources(1, &data.Source);
	if (handleAlutError("Unable to gen Sources"))
	{
		//TODO: throw exception
		return data;
	}

	// Now we set the buffer
	alSourcei (data.Source, AL_BUFFER,   data.Buffer   );
	if (handleAlutError("Unable to bind Buffer"))
	{
		//TODO: throw exception
		return data;
	}

	updateAlValues(data);

	// Do another error check and return.
	if (handleAlutError("Unknown Error"))
	{
		//TODO: throw exception
		return data;
	}


	dstream << "Everything went right" << std::endl;
	return data;
}

void Audio::updateListenerValues()
{
	alListenerfv(AL_POSITION,    m_ListenerPos);
	alListenerfv(AL_VELOCITY,    m_ListenerVel);
	alListenerfv(AL_ORIENTATION, m_ListenerOri);
}

void Audio::KillALData()
{
	for(
	    std::map<AudioSource*, AudioSourceData>::iterator it = m_audioSources.begin();
	    it != m_audioSources.end();
	    ++it
	)
	{
		//AudioSource* source = (*it).first;
		AudioSourceData data = (*it).second;
		alDeleteBuffers(1, &data.Buffer);
		alDeleteSources(1, &data.Source);
	}
	dstream << "Shutting down sound system" << std::endl;
	alutExit();
	m_state = ENDED;
}

bool Audio::handleAlutError(std::string errorMessage)
{
	ALuint error;
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		dstream << errorMessage << ": " << alutGetErrorString(error) << std::endl;
		m_state = ERROR;
		return true;
	}
	return false;
}

void Audio::updateAudioSourceData(AudioSource* source, AudioSourceData& data)
{
	data.Pitch = source->getPitch();
	data.Gain = source->getGain();
	v3f p = source->getPosition();
	data.SourcesPos[0] = p.X;
	data.SourcesPos[1] = p.Y;
	data.SourcesPos[2] = p.Z;
	v3f v = source->getVelocity();
	data.SourcesVel[0] = v.X;
	data.SourcesVel[1] = v.Y;
	data.SourcesVel[2] = v.Z;
	data.ReferenceDistance = source->getReferenceDistance();
	data.Looping = source->isLooping();
	data.Relative = source->isRelative();
}

void Audio::updateAlValues(AudioSourceData& data)
{
	alSourcef (data.Source, AL_PITCH,    data.Pitch    );
	alSourcef (data.Source, AL_GAIN,     data.Gain     );
	alSourcei (data.Source, AL_LOOPING, data.Looping);
	alSourcei (data.Source, AL_SOURCE_RELATIVE, data.Relative);
	alSourcef (data.Source, AL_REFERENCE_DISTANCE, data.ReferenceDistance);
	alSourcefv(data.Source, AL_POSITION, data.SourcesPos);
	alSourcefv(data.Source, AL_VELOCITY, data.SourcesVel);
	handleAlutError("Error while updating AL Values");
}
