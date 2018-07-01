#include "CanAnalyzerSettings.h"

#include <AnalyzerHelpers.h>
#include <sstream>
#include <cstring>

CanAnalyzerSettings::CanAnalyzerSettings()
:	mCanChannel ( UNDEFINED_CHANNEL ),
	mBitRate ( 1000000 ),
	mInverted( false )
{
	mCanChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mCanChannelInterface->SetTitleAndTooltip( "CAN", "Controller Area Network - Input" );
	mCanChannelInterface->SetChannel( mCanChannel );

	mBitRateInterface.reset( new AnalyzerSettingInterfaceInteger() );
	mBitRateInterface->SetTitleAndTooltip( "Bit Rate (Bits/s)",  "Specify the bit rate in bits per second." );
    mBitRateInterface->SetMax( 25000000 );
	mBitRateInterface->SetMin( 10000 );
	mBitRateInterface->SetInteger( mBitRate );

	mCanChannelInvertedInterface.reset( new AnalyzerSettingInterfaceBool( ) );
	mCanChannelInvertedInterface->SetTitleAndTooltip( "Inverted (CAN High)", "Use this option when recording CAN High directly" );
	mCanChannelInvertedInterface->SetValue( mInverted );


	AddInterface( mCanChannelInterface.get() );
	AddInterface( mBitRateInterface.get() );
	AddInterface( mCanChannelInvertedInterface.get() );

	//AddExportOption( 0, "Export as text/csv file", "text (*.txt);;csv (*.csv)" );
	AddExportOption( 0, "Export as text/csv file" );
	AddExportExtension( 0, "text", "txt" );
	AddExportExtension( 0, "csv", "csv" );

	ClearChannels();
	AddChannel( mCanChannel, "CAN", false );
}

CanAnalyzerSettings::~CanAnalyzerSettings()
{
}

bool CanAnalyzerSettings::SetSettingsFromInterfaces()
{
	Channel can_channel = mCanChannelInterface->GetChannel();
	
	if( can_channel == UNDEFINED_CHANNEL )
	{
		SetErrorText( "Please select a channel for the CAN interface" );
		return false;
	}
	mCanChannel = can_channel;
	mBitRate = mBitRateInterface->GetInteger();
	mInverted = mCanChannelInvertedInterface->GetValue();

	ClearChannels();
	AddChannel( mCanChannel, "CAN", true );

	return true;
}

void CanAnalyzerSettings::LoadSettings( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString( settings );

	const char* name_string;	//the first thing in the archive is the name of the protocol analyzer that the data belongs to.
	text_archive >> &name_string;
	if( strcmp( name_string, "SaleaeCANAnalyzer" ) != 0 )
		AnalyzerHelpers::Assert( "SaleaeCanAnalyzer: Provided with a settings string that doesn't belong to us;" );

	text_archive >>  mCanChannel;
	text_archive >>  mBitRate;
	text_archive >> mInverted; //SimpleArchive catches exception and returns false if it fails.

	ClearChannels();
	AddChannel( mCanChannel, "CAN", true );

	UpdateInterfacesFromSettings();
}

const char* CanAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive <<  "SaleaeCANAnalyzer";
	text_archive <<  mCanChannel;
	text_archive << mBitRate;
	text_archive << mInverted; 


	return SetReturnString( text_archive.GetString() );
}

void CanAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mCanChannelInterface->SetChannel( mCanChannel );
	mBitRateInterface->SetInteger( mBitRate );
	mCanChannelInvertedInterface->SetValue( mInverted );
}

BitState CanAnalyzerSettings::Recessive()
{
	if( mInverted )
		return BIT_LOW;
	return BIT_HIGH;
}
BitState CanAnalyzerSettings::Dominant()
{
	if( mInverted )
		return BIT_HIGH;
	return BIT_LOW;
}
