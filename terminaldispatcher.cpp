#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "terminaldispatcher.hpp"
#include "parseraction.hpp"

using namespace Terminal;

Dispatcher::Dispatcher()
  : params(), parsed_params(), parsed( false ), dispatch_chars(),
    OSC_string(), terminal_to_host()
{}

void Dispatcher::newparamchar( Parser::Param *act )
{
  assert( act->char_present );
  assert( (act->ch == ';') || ( (act->ch >= '0') && (act->ch <= '9') ) );
  if ( params.length() < 100 ) {
    /* enough for 16 five-char params plus 15 semicolons */
    params.push_back( act->ch );
    act->handled = true;
  }
  parsed = false;
}

void Dispatcher::collect( Parser::Collect *act )
{
  assert( act->char_present );
  if ( ( dispatch_chars.length() < 8 ) /* never should need more than 2 */
       && ( act->ch <= 255 ) ) {  /* ignore non-8-bit */    
    dispatch_chars.push_back( act->ch );
    act->handled = true;
  }
}

void Dispatcher::clear( Parser::Clear *act )
{
  params.clear();
  dispatch_chars.clear();
  parsed = false;
  act->handled = true;
}

void Dispatcher::parse_params( void )
{
  if ( parsed ) {
    return;
  }

  parsed_params.clear();
  const char *str = params.c_str();
  const char *segment_begin = str;

  while ( 1 ) {
    const char *segment_end = strchr( segment_begin, ';' );
    if ( segment_end == NULL ) {
      break;
    }

    errno = 0;
    char *endptr;
    int val = strtol( segment_begin, &endptr, 10 );
    if ( endptr == segment_begin ) {
      val = -1;
    }
    if ( errno == 0 ) {
      parsed_params.push_back( val );
    }

    segment_begin = segment_end + 1;
  }

  /* get last param */
  errno = 0;
  char *endptr;
  int val = strtol( segment_begin, &endptr, 10 );
  if ( endptr == segment_begin ) {
    val = -1;
  }
  if ( errno == 0 ) {
    parsed_params.push_back( val );
  }

  parsed = true;
}

int Dispatcher::getparam( size_t N, int defaultval )
{
  int ret = defaultval;
  if ( !parsed ) {
    parse_params();
  }

  if ( parsed_params.size() > N ) {
    ret = parsed_params[ N ];
  }
  if ( ret < 1 ) ret = defaultval;

  return ret;
}

int Dispatcher::param_count( void )
{
  if ( !parsed ) {
    parse_params();
  }

  return parsed_params.size();
}

std::string Dispatcher::str( void )
{
  char assum[ 64 ];
  snprintf( assum, 64, "[dispatch=\"%s\" params=\"%s\"]",
	    dispatch_chars.c_str(), params.c_str() );
  return std::string( assum );
}

static void register_function( Function_Type type,
			       std::string dispatch_chars,
			       Function f )
{
  switch ( type ) {
  case ESCAPE:
    global_dispatch_registry.escape.insert( dispatch_map_t::value_type( dispatch_chars, f ) );
    break;
  case CSI:
    global_dispatch_registry.CSI.insert( dispatch_map_t::value_type( dispatch_chars, f ) );
    break;
  case CONTROL:
    global_dispatch_registry.control.insert( dispatch_map_t::value_type( dispatch_chars, f ) );
    break;
  }
}

Function::Function( Function_Type type, std::string dispatch_chars,
		    void (*s_function)( Framebuffer *, Dispatcher * ) )
  : function( s_function )
{
  register_function( type, dispatch_chars, *this );
}

void Dispatcher::dispatch( Function_Type type, Parser::Action *act, Framebuffer *fb )
{
  /* add final char to dispatch key */
  if ( (type == ESCAPE) || (type == CSI) ) {
    assert( act->char_present );
    Parser::Collect act2;
    act2.char_present = true;
    act2.ch = act->ch;
    collect( &act2 ); 
  }

  dispatch_map_t *map = NULL;
  switch ( type ) {
  case ESCAPE:  map = &global_dispatch_registry.escape;  break;
  case CSI:     map = &global_dispatch_registry.CSI;     break;
  case CONTROL: map = &global_dispatch_registry.control; break;
  }

  std::string key = dispatch_chars;
  if ( type == CONTROL ) {
    assert( act->ch <= 255 );
    char ctrlstr[ 2 ] = { (char)act->ch, 0 };
    key = std::string( ctrlstr );
  }

  dispatch_map_t::const_iterator i = map->find( key );
  if ( i == map->end() ) {
    return;
  } else {
    act->handled = true;
    return i->second.function( fb, this );
  }
}

void Dispatcher::OSC_put( Parser::OSC_Put *act )
{
  assert( act->char_present );
  if ( OSC_string.size() < 256 ) { /* should be a long enough window title */
    OSC_string.push_back( act->ch );
    act->handled = true;
  }
}

void Dispatcher::OSC_start( Parser::OSC_Start *act )
{
  OSC_string.clear();
  act->handled = true;
}
