#include "ofxServerOscManager.h"

// ---------------------------------------------------------------------------------------------------------------------------------------------------
//
ofxServerOscManager::ofxServerOscManager()
{
	initialised = false;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
//
ofxServerOscManager::~ofxServerOscManager()
{
    ofRemoveListener(ofEvents().update, this, &ofxServerOscManager::_update);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
//
void ofxServerOscManager::init( string _xmlSettingsPath)
{
	string _serverSendHost	= "127.0.0.1";
	int _serverSendPort	= 7776;
	int _serverReceivePort	= 7777;

	ofxXmlSettings XML;
	bool loadedFile = XML.loadFile( _xmlSettingsPath );
	if( loadedFile )
	{
	     _serverSendHost = XML.getValue("Settings:Server:Host", "127.0.0.1");
             _serverSendPort = XML.getValue("Settings:Server:SendPost",	7776);
             _serverReceivePort = XML.getValue("Settings:Server:ReceivePort",	7777);
	}
	
	init( _serverSendHost, _serverSendPort, _serverReceivePort ); // init with default
	
}


// ---------------------------------------------------------------------------------------------------------------------------------------------
//

void ofxServerOscManager::init(string _serverSendHost, int _serverSendPort, int _serverReceivePort )
{
	serverSendHost	= _serverSendHost;
	serverSendPort	= _serverSendPort;
	serverReceivePort = _serverReceivePort;

	multicastSender.setup( serverSendHost, serverSendPort);
	receiver.setup( serverReceivePort );

	lastSentHelloMessageMillis = 0;
	milliseBetweenHelloMessages = 3 * 1000;

	ofAddListener(ofEvents().update, this, &ofxServerOscManager::_update );

	initialised = true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
//
void ofxServerOscManager::_update(ofEventArgs &e)
{

	// periodically send out a hello message on the multicast address, this way anyone joining us
	// can get the address to the server if they are on the same network and listening to the right port
	if( (getServerTime() - lastSentHelloMessageMillis) > milliseBetweenHelloMessages )
	{
		ofxOscMessage m;
		m.setAddress("/hello");
		m.addIntArg( serverReceivePort ); // add the port we would like to use to receive messages as an argument, seems more flexible than having a rule like remote port + 1
		// add the scene that we are in as well? Otherwise screens joining will not know what scene we are supposed to be in.

		multicastSender.sendMessage( m , false);

		lastSentHelloMessageMillis = getServerTime();
	}

	// check for waiting messages
	while( receiver.hasWaitingMessages() )
	{
		// get the next message
		ofxOscMessage m;
		receiver.getNextMessage(&m);

		if( m.getAddress() == "/ping" )
		{
            ofxOscSender & _sender = clients[m.getRemoteHost()].sender;
            _sender.setup(m.getRemoteHost(), serverSendPort);

            receivedMessageSubjects.push_back( m.getRemoteHost() );

			// we need to send a "pong" message back, either we send this over the multicasting address,
			// or we create a multicastSender for each new address and port that send us a message, I'm going to
			// try the multicasting route first.

			// their message comes in as /ping, their ID (int), their timestamp (int)

			int remoteComputerID 	= m.getArgAsInt32(0);
			int remoteComputerTime 	= m.getArgAsInt32(1);

			ofxOscMessage m;
			m.setAddress("/pong");
			m.addIntArg( remoteComputerID );		// their ID
			m.addIntArg( getServerTime() );			// my time
			m.addIntArg( remoteComputerTime );		// their time

			_sender.sendMessage(m, false);
            multicastSender.sendMessage(m, false);
		}
        if(m.getAddress() == "/data"){
            DataPacket packet;

			for(unsigned int i = 0; i < m.getNumArgs(); i++ )
			{
				ofxOscArgType argType = m.getArgType(i);
				if( argType == OFXOSC_TYPE_INT32 || argType ==  OFXOSC_TYPE_INT64 )
				{
					packet.valuesInt.push_back( m.getArgAsInt32(i) );
				}
				else if ( argType == OFXOSC_TYPE_FLOAT )
				{
					packet.valuesFloat.push_back( m.getArgAsFloat(i) );
				}
                else if( argType ==OFXOSC_TYPE_STRING)
                {
                    packet.valuesString.push_back( m.getArgAsString(i) );
                }
			}

            ofNotifyEvent( newDataEvent, packet, this );
        }
	}

	/*
     * current limit is 20 machines
     */

	int maxSavedSubjects = 20;
	if( receivedMessageSubjects.size() > maxSavedSubjects )
	{
		receivedMessageSubjects.erase( receivedMessageSubjects.begin(),receivedMessageSubjects.begin()+1 );
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
//
void ofxServerOscManager::draw()
{
	if( !initialised ) return;

	string buf;
	buf = "Sending OSC messages to " + string(serverSendHost) + ":"+ ofToString(serverSendPort) + "    Listening for OSC messages on port: " + ofToString(serverReceivePort);

#ifdef HEADLESS

#ifdef TARGET_LINUX
	std::system( "clear" );
#endif

	cout << "*********************************************************************************" << endl;
	cout << buf << endl;
	for( unsigned int i = 0; i < receivedMessageSubjects.size(); i++ )
	{
        cout << "    " << receivedMessageSubjects.at(i) << endl;
	}
	cout << "*********************************************************************************" << endl;
#else

	ofDrawBitmapString(buf, 10, 20);

	int i = 0;
	for( auto message : receivedMessageSubjects)
	{
		ofDrawBitmapString(message, 10, 60 + (i * 20) );
		i++;
	}


#endif
}


// ---------------------------------------------------------------------------------------------------------------------------------------------
//
void ofxServerOscManager::sendData( vector<string> _valuesStrings, vector<int> _valuesInt, vector<float> _valuesFloat )
{
	if( !initialised ) return;

	ofxOscMessage m;
	m.setAddress("/data");

    for(auto value : _valuesStrings)
	{
        m.addStringArg( value );
    }

	for(auto value : _valuesInt)
	{
		m.addIntArg( value );
	}

	for( auto value: _valuesFloat )
	{
		m.addFloatArg( value );
	}

    std::map<string, oscClient>::iterator iter;
    for(iter = clients.begin(); iter != clients.end(); iter++){
        iter->second.sender.sendMessage(m, false);
    }
}

void ofxServerOscManager::sendData(vector<string> _valuesStrings, vector<int> _valuesInt, vector<float> _valuesFloat, string clientID)
{
	if( !initialised ) return;

	ofxOscMessage m;
	m.setAddress("/data");

    for(auto value : _valuesStrings)
	{
        m.addStringArg( value );
    }

	for(auto value : _valuesInt)
	{
		m.addIntArg( value );
	}

	for( auto value: _valuesFloat )
	{
		m.addFloatArg( value );
	}

    clients[clientID].sender.sendMessage(m, false);
}

void ofxServerOscManager::sendData( DataPacket _packet)
{
	if( !initialised ) return;

	ofxOscMessage m;
	m.setAddress("/data");

    for(auto value : _packet.valuesString)
	{
        m.addStringArg( value );
    }

	for(auto value :  _packet.valuesInt)
	{
		m.addIntArg( value );
	}

	for( auto value:  _packet.valuesFloat)
	{
		m.addFloatArg( value );
	}
    

    std::map<string, oscClient>::iterator iter;
    for(iter = clients.begin(); iter != clients.end(); iter++){
        iter->second.sender.sendMessage(m, false);
    }
}

void ofxServerOscManager::sendData( DataPacket _packet, string clientID)
{
	if( !initialised ) return;

	ofxOscMessage m;
	m.setAddress("/data");

    for(auto value : _packet.valuesString)
	{
        m.addStringArg( value );
    }

	for(auto value :  _packet.valuesInt)
	{
		m.addIntArg( value );
	}

	for( auto value:  _packet.valuesFloat)
	{
		m.addFloatArg( value );
	}

    clients[clientID].sender.sendMessage(m, false);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
//
int ofxServerOscManager::getServerTime()
{
	return ofGetElapsedTimeMillis();
}
