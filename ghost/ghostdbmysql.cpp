/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#ifdef GHOST_MYSQL

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "ghostdb.h"
#include "ghostdbmysql.h"

#include <signal.h>

#ifdef WIN32
 #include <winsock.h>
#endif

#include <mysql/mysql.h>
#include <boost/thread.hpp>

//
// CGHostDBMySQL
//

CGHostDBMySQL :: CGHostDBMySQL( CConfig *CFG ) : CGHostDB( CFG )
{
	m_Server = CFG->GetString( "db_mysql_server", string( ) );
	m_Database = CFG->GetString( "db_mysql_database", "ghost" );
	m_User = CFG->GetString( "db_mysql_user", string( ) );
	m_Password = CFG->GetString( "db_mysql_password", string( ) );
	m_Port = CFG->GetInt( "db_mysql_port", 0 );
	m_BotID = CFG->GetInt( "db_mysql_botid", 0 );
	m_NumConnections = 1;
	m_OutstandingCallables = 0;

	mysql_library_init( 0, NULL, NULL );

	// create the first connection

	CONSOLE_Print( "[MYSQL] connecting to database server" );
	MYSQL *Connection = NULL;

	if( !( Connection = mysql_init( NULL ) ) )
	{
		CONSOLE_Print( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		m_Error = "error initializing MySQL connection";
		return;
	}

	my_bool Reconnect = true;
	mysql_options( Connection, MYSQL_OPT_RECONNECT, &Reconnect );

	if( !( mysql_real_connect( Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
	{
		CONSOLE_Print( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		m_Error = "error connecting to MySQL server";
		return;
	}

	m_IdleConnections.push( Connection );
}

CGHostDBMySQL :: ~CGHostDBMySQL( )
{
	CONSOLE_Print( "[MYSQL] closing " + UTIL_ToString( m_IdleConnections.size( ) ) + "/" + UTIL_ToString( m_NumConnections ) + " idle MySQL connections" );

	while( !m_IdleConnections.empty( ) )
	{
		mysql_close( (MYSQL *)m_IdleConnections.front( ) );
		m_IdleConnections.pop( );
	}

	if( m_OutstandingCallables > 0 )
		CONSOLE_Print( "[MYSQL] " + UTIL_ToString( m_OutstandingCallables ) + " outstanding callables were never recovered" );

	mysql_library_end( );
}

string CGHostDBMySQL :: GetStatus( )
{
	return "DB STATUS --- Connections: " + UTIL_ToString( m_IdleConnections.size( ) ) + "/" + UTIL_ToString( m_NumConnections ) + " idle. Outstanding callables: " + UTIL_ToString( m_OutstandingCallables ) + ".";
}

void CGHostDBMySQL :: RecoverCallable( CBaseCallable *callable )
{
	CMySQLCallable *MySQLCallable = dynamic_cast<CMySQLCallable *>( callable );

	if( MySQLCallable )
	{
		if( m_IdleConnections.size( ) > 30 )
		{
			mysql_close( (MYSQL *)MySQLCallable->GetConnection( ) );
			m_NumConnections--;
		}
		else
			m_IdleConnections.push( MySQLCallable->GetConnection( ) );

		if( m_OutstandingCallables == 0 )
			CONSOLE_Print( "[MYSQL] recovered a mysql callable with zero outstanding" );
		else
			m_OutstandingCallables--;

		if( !MySQLCallable->GetError( ).empty( ) )
			CONSOLE_Print( "[MYSQL] error --- " + MySQLCallable->GetError( ) );
	}
	else
		CONSOLE_Print( "[MYSQL] tried to recover a non-mysql callable" );
}

void CGHostDBMySQL :: CreateThread( CBaseCallable *callable )
{
	try
	{
		boost :: thread Thread( boost :: ref( *callable ) );
	}
	catch( boost :: thread_resource_error tre )
	{
		CONSOLE_Print( "[MYSQL] error spawning thread on attempt #1 [" + string( tre.what( ) ) + "], pausing execution and trying again in 50ms" );
		MILLISLEEP( 50 );

		try
		{
			boost :: thread Thread( boost :: ref( *callable ) );
		}
		catch( boost :: thread_resource_error tre2 )
		{
			CONSOLE_Print( "[MYSQL] error spawning thread on attempt #2 [" + string( tre2.what( ) ) + "], giving up" );
			callable->SetReady( true );
		}
	}
}

CCallableAdminCount *CGHostDBMySQL :: ThreadedAdminCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminCount *Callable = new CMySQLCallableAdminCount( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminCheck *CGHostDBMySQL :: ThreadedAdminCheck( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminCheck *Callable = new CMySQLCallableAdminCheck( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminAdd *CGHostDBMySQL :: ThreadedAdminAdd( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminAdd *Callable = new CMySQLCallableAdminAdd( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminRemove *CGHostDBMySQL :: ThreadedAdminRemove( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminRemove *Callable = new CMySQLCallableAdminRemove( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminList *CGHostDBMySQL :: ThreadedAdminList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminList *Callable = new CMySQLCallableAdminList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanCount *CGHostDBMySQL :: ThreadedBanCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanCount *Callable = new CMySQLCallableBanCount( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanCheck *CGHostDBMySQL :: ThreadedBanCheck( string server, string user, string ip )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanCheck *Callable = new CMySQLCallableBanCheck( server, user, ip, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanAdd *CGHostDBMySQL :: ThreadedBanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t bantime, uint32_t ipban )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanAdd *Callable = new CMySQLCallableBanAdd( server, user, ip, gamename, admin, reason, bantime, ipban, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanRemove *CGHostDBMySQL :: ThreadedBanRemove( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanRemove *Callable = new CMySQLCallableBanRemove( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanRemove *CGHostDBMySQL :: ThreadedBanRemove( string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanRemove *Callable = new CMySQLCallableBanRemove( string( ), user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanList *CGHostDBMySQL :: ThreadedBanList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanList *Callable = new CMySQLCallableBanList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGameAdd *CGHostDBMySQL :: ThreadedGameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGameAdd *Callable = new CMySQLCallableGameAdd( server, map, gamename, ownername, duration, gamestate, creatorname, creatorserver, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGamePlayerAdd *CGHostDBMySQL :: ThreadedGamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGamePlayerAdd *Callable = new CMySQLCallableGamePlayerAdd( gameid, name, ip, spoofed, spoofedrealm, reserved, loadingtime, left, leftreason, team, colour, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableRegisterPlayerAdd *CGHostDBMySQL :: ThreadedRegisterPlayerAdd( string name, string email, string ip )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableRegisterPlayerAdd *Callable = new CMySQLCallableRegisterPlayerAdd( name, email, ip, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGamePlayerSummaryCheck *CGHostDBMySQL :: ThreadedGamePlayerSummaryCheck( string name )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGamePlayerSummaryCheck *Callable = new CMySQLCallableGamePlayerSummaryCheck( name, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAGameAdd *CGHostDBMySQL :: ThreadedDotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAGameAdd *Callable = new CMySQLCallableDotAGameAdd( gameid, winner, min, sec, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAEventAdd *CGHostDBMySQL :: ThreadedDotAEventAdd( uint32_t gameid, string killer, string victim, uint32_t kcolour, uint32_t vcolour )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAEventAdd *Callable = new CMySQLCallableDotAEventAdd( gameid, killer, victim, kcolour, vcolour, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAPlayerAdd *CGHostDBMySQL :: ThreadedDotAPlayerAdd( uint32_t gameid, string name, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills, uint32_t outcome, uint32_t level, uint32_t apm )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAPlayerAdd *Callable = new CMySQLCallableDotAPlayerAdd( gameid, name, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, hero, newcolour, towerkills, raxkills, courierkills, outcome, level, apm, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAPlayerSummaryCheck *CGHostDBMySQL :: ThreadedDotAPlayerSummaryCheck( string name )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAPlayerSummaryCheck *Callable = new CMySQLCallableDotAPlayerSummaryCheck( name, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDownloadAdd *CGHostDBMySQL :: ThreadedDownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDownloadAdd *Callable = new CMySQLCallableDownloadAdd( map, mapsize, name, ip, spoofed, spoofedrealm, downloadtime, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableScoreCheck *CGHostDBMySQL :: ThreadedScoreCheck( string category, string name, string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableScoreCheck *Callable = new CMySQLCallableScoreCheck( category, name, server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDPlayerAdd *CGHostDBMySQL :: ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDPlayerAdd *Callable = new CMySQLCallableW3MMDPlayerAdd( category, gameid, pid, name, flag, leaver, practicing, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_ints, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_reals, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_strings, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableUpdateGameInfo *CGHostDBMySQL :: ThreadedUpdateGameInfo( string name, uint32_t players, bool ispublic )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableUpdateGameInfo *Callable = new CMySQLCallableUpdateGameInfo( name, players, ispublic, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

void *CGHostDBMySQL :: GetIdleConnection( )
{
	void *Connection = NULL;

	if( !m_IdleConnections.empty( ) )
	{
		Connection = m_IdleConnections.front( );
		m_IdleConnections.pop( );
	}

	return Connection;
}

//
// unprototyped global helper functions
//

string MySQLEscapeString( void *conn, string str )
{
	char *to = new char[str.size( ) * 2 + 1];
	unsigned long size = mysql_real_escape_string( (MYSQL *)conn, to, str.c_str( ), str.size( ) );
	string result( to, size );
	delete [] to;
	return result;
}

vector<string> MySQLFetchRow( MYSQL_RES *res )
{
	vector<string> Result;

	MYSQL_ROW Row = mysql_fetch_row( res );

	if( Row )
	{
		unsigned long *Lengths;
		Lengths = mysql_fetch_lengths( res );

		for( unsigned int i = 0; i < mysql_num_fields( res ); i++ )
		{
			if( Row[i] )
				Result.push_back( string( Row[i], Lengths[i] ) );
			else
				Result.push_back( string( ) );
		}
	}

	return Result;
}

//
// global helper functions
//

uint32_t MySQLAdminCount( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM admins WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting admins [" + server + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

bool MySQLAdminCheck( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool IsAdmin = false;
	string Query = "SELECT * FROM admins WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
				IsAdmin = true;

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return IsAdmin;
}

bool MySQLAdminAdd( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "INSERT INTO admins ( botid, server, name ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscServer + "', '" + EscUser + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLAdminRemove( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM admins WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

vector<string> MySQLAdminList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<string> AdminList;
	string Query = "SELECT name FROM admins WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
				AdminList.push_back( Row[0] );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return AdminList;
}

uint32_t MySQLBanCount( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM bans WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting bans [" + server + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

CDBBan *MySQLBanCheck( void *conn, string *error, uint32_t botid, string server, string user, string ip )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscIP = MySQLEscapeString( conn, ip );
	CDBBan *Ban = NULL;
	string Query;

	if( ip.empty( ) )
		Query = "SELECT name, ip, DATE(date), gamename, admin, reason, ipban, IF(expires IS NULL, 0, 1) as temp, FROM_UNIXTIME(expires) FROM bans WHERE name LIKE '" + EscUser + "'";
	else
		Query = "SELECT name, ip, DATE(date), gamename, admin, reason, ipban, IF(expires IS NULL, 0, 1) as temp, FROM_UNIXTIME(expires) FROM bans WHERE name LIKE '" + EscUser + "' OR ip='" + EscIP + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 9 )
			{
				uint32_t temp = UTIL_ToUInt32(Row[7]);
				string expires;

				if (temp == 1)
					expires = Row[8];

				Ban = new CDBBan( server, Row[0], Row[1], Row[2], Row[3], Row[4], Row[5], UTIL_ToUInt32(Row[6]), expires );
			}
			/* else
				*error = "error checking ban [" + server + " : " + user + "] - row doesn't have 6 columns"; */

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Ban;
}

bool MySQLBanAdd( void *conn, string *error, uint32_t botid, string server, string user, string ip, string gamename, string admin, string reason, uint32_t bantime, uint32_t ipban )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscGameName = MySQLEscapeString( conn, gamename );
	string EscAdmin = MySQLEscapeString( conn, admin );
	string EscReason = MySQLEscapeString( conn, reason );
	bool Success = false;
	string Query;

	/*
		NordicLeague - @begin - hack to enable both regular bans and ip-bans side-by-side
	*/

	if (ipban != 0 && ip.empty())
	{
		// we need to find the last used ip to fill in since we dont know the users current ip

		string QFindIp = "SELECT ip FROM gameplayers WHERE name LIKE '" + user + "' ORDER BY id DESC LIMIT 1";

		if( mysql_real_query( (MYSQL *)conn, QFindIp.c_str( ), QFindIp.size( ) ) != 0 )
			*error = mysql_error( (MYSQL *)conn );
		else
		{
                	MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

        	        if( Result )
	                {
                        	vector<string> Row = MySQLFetchRow( Result );

                	        if( Row.size( ) == 1 )
        	                        ip = Row[0];
	                        else
							{
                                	*error = "error finding any suitable ip for [" + user + "]";
									mysql_free_result( Result );
									return false;
							}

                        	mysql_free_result( Result );

                	}
        	        else
	                        *error = mysql_error( (MYSQL *)conn );
	
		}
	}

	string EscIP = MySQLEscapeString( conn, ip );

	/*
		NordicLeague - @end - hack to enable both regular bans and ip-bans side-by-side
	*/
	
	if (bantime > 0)
		Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, expires, ipban ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', NOW( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "', UNIX_TIMESTAMP() + " + UTIL_ToString(bantime) + ", " + UTIL_ToString(ipban) + " )";
	else
		Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, ipban ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', NOW( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "', " + UTIL_ToString(ipban) + " )";
	
	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM bans WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM bans WHERE name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

vector<CDBBan *> MySQLBanList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<CDBBan *> BanList;
	string Query = "SELECT name, ip, DATE(date), gamename, admin, reason, ipban, IF(expires IS NULL, 0, 1) as temp, FROM_UNIXTIME(expires) FROM bans WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( Row.size( ) == 9 )
			{
				if (UTIL_ToUInt32(Row[7]))
					BanList.push_back( new CDBBan( server, Row[0], Row[1], Row[2], Row[3], Row[4], Row[5], UTIL_ToUInt32(Row[6]), Row[8] ) );
				else
					BanList.push_back( new CDBBan( server, Row[0], Row[1], Row[2], Row[3], Row[4], Row[5], UTIL_ToUInt32(Row[6]), string() ) );
					
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return BanList;
}

uint32_t MySQLGameAdd( void *conn, string *error, uint32_t botid, string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver )
{
	uint32_t RowID = 0;
	string EscServer = MySQLEscapeString( conn, server );
	string EscMap = MySQLEscapeString( conn, map );
	string EscGameName = MySQLEscapeString( conn, gamename );
	string EscOwnerName = MySQLEscapeString( conn, ownername );
	string EscCreatorName = MySQLEscapeString( conn, creatorname );
	string EscCreatorServer = MySQLEscapeString( conn, creatorserver );
	string Query = "INSERT INTO games ( botid, server, map, datetime, gamename, ownername, duration, gamestate, creatorname, creatorserver ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscServer + "', '" + EscMap + "', NOW( ), '" + EscGameName + "', '" + EscOwnerName + "', " + UTIL_ToString( duration ) + ", " + UTIL_ToString( gamestate ) + ", '" + EscCreatorName + "', '" + EscCreatorServer + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

uint32_t MySQLGamePlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t RowID = 0;
	string EscName = MySQLEscapeString( conn, name );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscSpoofedRealm = MySQLEscapeString( conn, spoofedrealm );
	string EscLeftReason = MySQLEscapeString( conn, leftreason );
	string Query = "INSERT INTO gameplayers ( botid, gameid, name, ip, spoofed, reserved, loadingtime, `left`, leftreason, team, colour, spoofedrealm ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", '" + EscName + "', '" + EscIP + "', " + UTIL_ToString( spoofed ) + ", " + UTIL_ToString( reserved ) + ", " + UTIL_ToString( loadingtime ) + ", " + UTIL_ToString( left ) + ", '" + EscLeftReason + "', " + UTIL_ToString( team ) + ", " + UTIL_ToString( colour ) + ", '" + EscSpoofedRealm + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

uint32_t MySQLRegisterPlayerAdd( void *conn, string *error, string name, string email, string ip )
{
	string EscName = MySQLEscapeString( conn, name );
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t RowID = 0;
	string EscEmail = MySQLEscapeString( conn, email );
	string EscIP = MySQLEscapeString( conn, ip );

	string QueryCheck = "SELECT name from registration where LOWER(name) = LOWER('" + EscName + "')";

	if( mysql_real_query( (MYSQL *)conn, QueryCheck.c_str( ), QueryCheck.size( ) ) != 0 )
	{
		*error = mysql_error( (MYSQL *)conn );
		return 0;
	}
	else
	{
		//RowID = mysql_insert_id( (MYSQL *)conn );
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );
		
		if (Result)
		{
			vector<string> Row = MySQLFetchRow( Result );
			mysql_free_result( Result );

			if (Row.size( ) == 1)
				return 1;
		}
	}

	string Query = "INSERT INTO registration ( name, email, ip ) VALUES ( '" + EscName + "', '" + EscEmail + "', '" + EscIP + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

CDBGamePlayerSummary *MySQLGamePlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscName = MySQLEscapeString( conn, name );
	CDBGamePlayerSummary *GamePlayerSummary = NULL;
	string Query = "SELECT MIN(DATE(datetime)), MAX(DATE(datetime)), COUNT(*), MIN(loadingtime), AVG(loadingtime), MAX(loadingtime), MIN(`left`/duration)*100, AVG(`left`/duration)*100, MAX(`left`/duration)*100, MIN(duration), AVG(duration), MAX(duration) FROM gameplayers LEFT JOIN games ON games.id=gameid WHERE name LIKE '" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 12 )
			{
				string FirstGameDateTime = Row[0];
				string LastGameDateTime = Row[1];
				uint32_t TotalGames = UTIL_ToUInt32( Row[2] );
				uint32_t MinLoadingTime = UTIL_ToUInt32( Row[3] );
				uint32_t AvgLoadingTime = UTIL_ToUInt32( Row[4] );
				uint32_t MaxLoadingTime = UTIL_ToUInt32( Row[5] );
				uint32_t MinLeftPercent = UTIL_ToUInt32( Row[6] );
				uint32_t AvgLeftPercent = UTIL_ToUInt32( Row[7] );
				uint32_t MaxLeftPercent = UTIL_ToUInt32( Row[8] );
				uint32_t MinDuration = UTIL_ToUInt32( Row[9] );
				uint32_t AvgDuration = UTIL_ToUInt32( Row[10] );
				uint32_t MaxDuration = UTIL_ToUInt32( Row[11] );
				GamePlayerSummary = new CDBGamePlayerSummary( string( ), name, FirstGameDateTime, LastGameDateTime, TotalGames, MinLoadingTime, AvgLoadingTime, MaxLoadingTime, MinLeftPercent, AvgLeftPercent, MaxLeftPercent, MinDuration, AvgDuration, MaxDuration );
			}
			else
				*error = "error checking gameplayersummary [" + name + "] - row doesn't have 12 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return GamePlayerSummary;
}

CDBGamePlayerSummary *MySQLReducedGamePlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscName = MySQLEscapeString( conn, name );
	CDBGamePlayerSummary *GamePlayerSummary = NULL;
//	string Query = "SELECT MIN(DATE(datetime)), MAX(DATE(datetime)), COUNT(*), MIN(loadingtime), AVG(loadingtime), MAX(loadingtime), MIN(`left`/duration)*100, AVG(`left`/duration)*100, MAX(`left`/duration)*100, MIN(duration), AVG(duration), MAX(duration) FROM gameplayers LEFT JOIN games ON games.id=gameid WHERE name LIKE '" + EscName + "'";
	string Query = "SELECT COUNT(*), AVG(`left`/duration)*100 FROM gameplayers LEFT JOIN games ON games.id=gameid WHERE name LIKE '" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 2 )
			{
				uint32_t TotalGames = UTIL_ToUInt32( Row[0] );
				uint32_t AvgLeftPercent = UTIL_ToUInt32( Row[7] );
				GamePlayerSummary = new CDBGamePlayerSummary( string( ), name, string( ), string( ), TotalGames, 0, 0, 0, 0, AvgLeftPercent, 0, 0, 0, 0 );
			}
			else
				*error = "error checking gameplayersummary [" + name + "] - row doesn't have 12 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return GamePlayerSummary;
}

uint32_t MySQLDotAGameAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	uint32_t RowID = 0;
	string Query = "INSERT INTO dotagames ( botid, gameid, winner, min, sec ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( winner ) + ", " + UTIL_ToString( min ) + ", " + UTIL_ToString( sec ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

uint32_t MySQLDotAEventAdd( void *conn, string *error, uint32_t gameid, string killer, string victim, uint32_t kcolour, uint32_t vcolour )
{
	uint32_t RowID = 0;
	string Query = "INSERT INTO dotaevents ( killer, victim, kcolour, vcolour ) VALUES ( '" + killer + "', '" + victim + "', " + UTIL_ToString(kcolour) + ", " + UTIL_ToString(vcolour) + ")";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

uint32_t MySQLDotAPlayerAdd( void *conn, string *error, uint32_t botid, string name, uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t 
assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, 
uint32_t courierkills, uint32_t outcome, uint32_t level, uint32_t apm )
{
	uint32_t AffectedRows = 0;
	string EscItem1 = MySQLEscapeString( conn, item1 );
	string EscItem2 = MySQLEscapeString( conn, item2 );
	string EscItem3 = MySQLEscapeString( conn, item3 );
	string EscItem4 = MySQLEscapeString( conn, item4 );
	string EscItem5 = MySQLEscapeString( conn, item5 );
	string EscItem6 = MySQLEscapeString( conn, item6 );
	string EscHero = MySQLEscapeString( conn, hero );
	string EscName = MySQLEscapeString( conn, name );

	string Query = "CALL UpdateDotaPlayerApm(" + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( colour ) + ", " + UTIL_ToString( kills ) + ", " + UTIL_ToString( deaths ) + ", " + UTIL_ToString( creepkills ) + ", " + UTIL_ToString( creepdenies ) + ", " + UTIL_ToString( assists ) + ", " + UTIL_ToString( gold ) + ", " + UTIL_ToString( neutralkills ) + ", '" + EscItem1 + "', '" + EscItem2 + "', '" + EscItem3 + "', '" + EscItem4 + "', '" + EscItem5 + "', '" + EscItem6 + "', '" + EscHero + "', " + UTIL_ToString( newcolour ) + ", " + UTIL_ToString( towerkills ) + ", " + UTIL_ToString( raxkills ) + ", " + UTIL_ToString( courierkills ) + ", " + UTIL_ToString( outcome ) + ", " + UTIL_ToString( level ) + ", " + UTIL_ToString( apm ) + ", '" + EscName + "', '')";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		AffectedRows = mysql_affected_rows( (MYSQL *)conn );

	return AffectedRows;
}

CDBDotAPlayerSummary *MySQLDotAPlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscName = MySQLEscapeString( conn, name );
	CDBDotAPlayerSummary *DotAPlayerSummary = NULL;
	//string Query = "SELECT COUNT(dotaplayers.id), SUM(kills), SUM(deaths), SUM(creepkills), SUM(creepdenies), SUM(assists), SUM(neutralkills), SUM(towerkills), SUM(raxkills), SUM(courierkills) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour WHERE LOWER(name)='" + EscName + "'";

	string Query = "SELECT total_wins, total_losses, total_draws, total_kills, total_deaths, total_creepkills, total_creepdenies, total_assists, total_neutralkills, total_towerkills, total_raxkills, total_courierkills, streak FROM dotastats WHERE player_name='" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result && Result->row_count > 0)
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 13 )
			{
				uint32_t TotalGames = UTIL_ToUInt32( Row[0] ) + UTIL_ToUInt32( Row[1] ) + UTIL_ToUInt32( Row[2] );

				if( TotalGames > 0 )
				{

					uint32_t TotalWins = UTIL_ToUInt32( Row[0] );
					uint32_t TotalLosses = UTIL_ToUInt32( Row[1] );
					uint32_t TotalKills = UTIL_ToUInt32( Row[3] );
					uint32_t TotalDeaths = UTIL_ToUInt32( Row[4] );
					uint32_t TotalCreepKills = UTIL_ToUInt32( Row[5] );
					uint32_t TotalCreepDenies = UTIL_ToUInt32( Row[6] );
					uint32_t TotalAssists = UTIL_ToUInt32( Row[7] );
					uint32_t TotalNeutralKills = UTIL_ToUInt32( Row[8] );
					uint32_t TotalTowerKills = UTIL_ToUInt32( Row[9] );
					uint32_t TotalRaxKills = UTIL_ToUInt32( Row[10] );
					uint32_t TotalCourierKills = UTIL_ToUInt32( Row[11] );
					uint32_t Streak = UTIL_ToUInt32( Row[12] );

					double Score = -100000.0;
					uint32_t Rank = 0;
					
					// calculate score and rank

					string Query4 = "SELECT score FROM dota_elo_scores WHERE name='" + EscName + "'";
					
					//CONSOLE_Print( "[MYSQL] statsdota: " + Query4 );

					if( mysql_real_query( (MYSQL *)conn, Query4.c_str( ), Query4.size( ) ) != 0 )
						*error = mysql_error( (MYSQL *)conn );
					else
					{
						MYSQL_RES *Result4 = mysql_store_result( (MYSQL *)conn );

						if( Result4 )
						{
							vector<string> Row4 = MySQLFetchRow( Result4 );

							if( Row4.size( ) == 1 )
								Score = UTIL_ToDouble( Row4[0] );
							else
								*error = "error checking dotaplayersummary score [" + name + "] - row doesn't have 1 column";

							mysql_free_result( Result4 );
						}
						else
							*error = mysql_error( (MYSQL *)conn );
					}

					// calculate rank

					string Query5 = "select COUNT(score) from dota_elo_scores where score >= " + UTIL_ToString(Score, 2);

					if( mysql_real_query( (MYSQL *)conn, Query5.c_str( ), Query5.size( ) ) != 0 )
						*error = mysql_error( (MYSQL *)conn );
					else
					{
						MYSQL_RES *Result5 = mysql_store_result( (MYSQL *)conn );
						if( Result5 )
						{
							vector<string> Row5 = MySQLFetchRow( Result5 );
							if( Row5.size( ) == 1 )
								Rank = UTIL_ToUInt32( Row5[0] );
							else
								*error = "error checking dotaplayersummary rank [" + name + "] - row doesn't have 1 column";

							mysql_free_result( Result5 );
						}
						else
							*error = mysql_error( (MYSQL *)conn );
					}
					
					// done
					DotAPlayerSummary = new CDBDotAPlayerSummary( string( ), name, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills, Rank, Score, Streak );
				}
			}
			else
				*error = "error checking dotaplayersummary [" + name + "] - row doesn't have 10 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return DotAPlayerSummary;
}

bool MySQLDownloadAdd( void *conn, string *error, uint32_t botid, string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	bool Success = false;
	string EscMap = MySQLEscapeString( conn, map );
	string EscName = MySQLEscapeString( conn, name );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscSpoofedRealm = MySQLEscapeString( conn, spoofedrealm );
	string Query = "INSERT INTO downloads ( botid, map, mapsize, datetime, name, ip, spoofed, spoofedrealm, downloadtime ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscMap + "', " + UTIL_ToString( mapsize ) + ", NOW( ), '" + EscName + "', '" + EscIP + "', " + UTIL_ToString( spoofed ) + ", '" + EscSpoofedRealm + "', " + UTIL_ToString( downloadtime ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

double MySQLScoreCheck( void *conn, string *error, uint32_t botid, string category, string name, string server )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscCategory = MySQLEscapeString( conn, category );
	string EscName = MySQLEscapeString( conn, name );
	string EscServer = MySQLEscapeString( conn, server );
	double Score = -100000.0;
	string Query = "SELECT score FROM dota_elo_scores WHERE name='" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Score = UTIL_ToDouble( Row[0] );
			/* else
				*error = "error checking score [" + category + " : " + name + " : " + server + "] - row doesn't have 1 column"; */

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Score;
}

uint32_t MySQLW3MMDPlayerAdd( void *conn, string *error, uint32_t botid, string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t RowID = 0;
	string EscCategory = MySQLEscapeString( conn, category );
	string EscName = MySQLEscapeString( conn, name );
	string EscFlag = MySQLEscapeString( conn, flag );
	string Query = "INSERT INTO w3mmdplayers ( botid, category, gameid, pid, name, flag, leaver, practicing ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscCategory + "', " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( pid ) + ", '" + EscName + "', '" + EscFlag + "', " + UTIL_ToString( leaver ) + ", " + UTIL_ToString( practicing ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,int32_t> var_ints )
{
	if( var_ints.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,int32_t> :: iterator i = var_ints.begin( ); i != var_ints.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );

		if( Query.empty( ) )
			Query = "INSERT INTO w3mmdvars ( botid, gameid, pid, varname, value_int ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second ) + " )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second ) + " )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,double> var_reals )
{
	if( var_reals.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,double> :: iterator i = var_reals.begin( ); i != var_reals.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );

		if( Query.empty( ) )
			Query = "INSERT INTO w3mmdvars ( botid, gameid, pid, varname, value_real ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second, 10 ) + " )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second, 10 ) + " )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,string> var_strings )
{
	if( var_strings.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,string> :: iterator i = var_strings.begin( ); i != var_strings.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );
		string EscValueString = MySQLEscapeString( conn, i->second );

		if( Query.empty( ) )
			Query = "INSERT INTO w3mmdvars ( botid, gameid, pid, varname, value_string ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', '" + EscValueString + "' )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', '" + EscValueString + "' )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLUpdateGameInfo( void *conn, string *error, uint32_t botid, string name, uint32_t players, bool ispublic )
{
		
	string Query;
	string EscName = MySQLEscapeString( conn, name );
	uint32_t IsPublic = ispublic ? 1 : 0;

	if ( name.empty() && players == 255 )
		Query = "DELETE FROM gameinfo WHERE botid = " + UTIL_ToString( botid );	
	else if ( players == 255 )
		Query = "DELETE FROM gameinfo WHERE name = '" + EscName + "' AND botid = " + UTIL_ToString( botid );
	else
		Query = "INSERT INTO gameinfo (name, players, botid, public) VALUES ('" + EscName + "', " + UTIL_ToString( players ) + ", " + UTIL_ToString( botid ) + ", " + UTIL_ToString( IsPublic ) + ") ON DUPLICATE KEY UPDATE players=" + UTIL_ToString( players ) + ", public=" + UTIL_ToString( IsPublic ) + ";";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
	{
		*error = mysql_error( (MYSQL *)conn );
		return false;
	}

	return true;
}


//
// MySQL Callables
//

void CMySQLCallable :: Init( )
{
	CBaseCallable :: Init( );

#ifndef WIN32
	// disable SIGPIPE since this is (or should be) a new thread and it doesn't inherit the spawning thread's signal handlers
	// MySQL should automatically disable SIGPIPE when we initialize it but we do so anyway here

	signal( SIGPIPE, SIG_IGN );
#endif

	mysql_thread_init( );

	if( !m_Connection )
	{
		if( !( m_Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)m_Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)m_Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)m_Connection, m_SQLServer.c_str( ), m_SQLUser.c_str( ), m_SQLPassword.c_str( ), m_SQLDatabase.c_str( ), m_SQLPort, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)m_Connection );
	}
	else if( mysql_ping( (MYSQL *)m_Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)m_Connection );
}

void CMySQLCallable :: Close( )
{
	mysql_thread_end( );

	CBaseCallable :: Close( );
}

void CMySQLCallableAdminCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminCount( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableAdminCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminCheck( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableBanCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanCount( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableBanCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanCheck( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_IP );

	Close( );
}

void CMySQLCallableBanAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_IP, m_GameName, m_Admin, m_Reason, m_BanTime, m_IPBan );

	Close( );
}

void CMySQLCallableBanRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		if( m_Server.empty( ) )
			m_Result = MySQLBanRemove( m_Connection, &m_Error, m_SQLBotID, m_User );
		else
			m_Result = MySQLBanRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );
	}

	Close( );
}

void CMySQLCallableBanList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableGameAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGameAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_Map, m_GameName, m_OwnerName, m_Duration, m_GameState, m_CreatorName, m_CreatorServer );

	Close( );
}

void CMySQLCallableGamePlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGamePlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Name, m_IP, m_Spoofed, m_SpoofedRealm, m_Reserved, m_LoadingTime, m_Left, m_LeftReason, m_Team, m_Colour );

	Close( );
}

void CMySQLCallableRegisterPlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLRegisterPlayerAdd( m_Connection, &m_Error, m_Name, m_Email, m_IP );

	Close( );
}

void CMySQLCallableGamePlayerSummaryCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGamePlayerSummaryCheck( m_Connection, &m_Error, m_SQLBotID, m_Name );

	Close( );
}

void CMySQLCallableDotAGameAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAGameAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Winner, m_Min, m_Sec );

	Close( );
}

void CMySQLCallableDotAEventAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAEventAdd( m_Connection, &m_Error, m_GameID, m_Killer, m_Victim, m_KillerColour, m_VictimColour );

	Close( );
}

void CMySQLCallableDotAPlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAPlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_Name, m_GameID, m_Colour, m_Kills, m_Deaths, m_CreepKills, m_CreepDenies, m_Assists, m_Gold, m_NeutralKills, m_Item1, 
m_Item2, m_Item3, m_Item4, m_Item5, m_Item6, m_Hero, m_NewColour, m_TowerKills, m_RaxKills, m_CourierKills, m_Outcome, m_Level, m_Apm );

	Close( );
}

void CMySQLCallableDotAPlayerSummaryCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAPlayerSummaryCheck( m_Connection, &m_Error, m_SQLBotID, m_Name );

	Close( );
}

void CMySQLCallableDownloadAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDownloadAdd( m_Connection, &m_Error, m_SQLBotID, m_Map, m_MapSize, m_Name, m_IP, m_Spoofed, m_SpoofedRealm, m_DownloadTime );

	Close( );
}

void CMySQLCallableScoreCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		m_GamePlayer = MySQLGamePlayerSummaryCheck( m_Connection, &m_Error, m_SQLBotID, m_Name );
		m_Result = MySQLScoreCheck( m_Connection, &m_Error, m_SQLBotID, m_Category, m_Name, m_Server );
	}
	Close( );
}

void CMySQLCallableW3MMDPlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLW3MMDPlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_Category, m_GameID, m_PID, m_Name, m_Flag, m_Leaver, m_Practicing );

	Close( );
}

void CMySQLCallableW3MMDVarAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		if( m_ValueType == VALUETYPE_INT )
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarInts );
		else if( m_ValueType == VALUETYPE_REAL )
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarReals );
		else
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarStrings );
	}

	Close( );
}

void CMySQLCallableUpdateGameInfo :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLUpdateGameInfo( m_Connection, &m_Error, m_SQLBotID, m_Name, m_Players, m_Public );

	Close( );
}

#endif
