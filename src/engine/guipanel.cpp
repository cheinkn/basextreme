
#include "headers.h"
#include "gui.h"

GuiPanel::GuiPanel()
{
    _name = "Noname";
    _rect.left = 0, _rect.top = 0, _rect.right = 32, _rect.bottom = 32;
    _textureRect.left = 0, _textureRect.top = 0, _textureRect.right = 32, _textureRect.right = 32;
    _texture = NULL;
    _color = D3DCOLOR_RGBA( 255,255,255,255 );
    _parent = NULL;
    _visible = true;
    _renderCallback = NULL;
    _renderCallbackData = NULL;
}

GuiPanel::GuiPanel(const char* panelName)
{	
    _name = panelName;
    _rect.left = 0, _rect.top = 0, _rect.right = 32, _rect.bottom = 32;
    _textureRect.left = 0, _textureRect.top = 0, _textureRect.right = 32, _textureRect.right = 32;
    _texture = NULL;
    _color = D3DCOLOR_RGBA( 255,255,255,255 );
    _parent = NULL;
    _visible = true;
    _renderCallback = NULL;
    _renderCallbackData = NULL;
}

GuiPanel::~GuiPanel()
{
    Gui::instance->notifyPanelDestroy( this );
    if( _parent ) _parent->removePanel( this );

    GuiPanel* child;
    while( _children.size() ) 
    {
        child = *_children.begin();
        removePanel( child );
        child->release();
    }
}

void GuiPanel::initializePanel(TiXmlNode* node)
{
    assert( node->Type() == TiXmlNode::ELEMENT );

    // initialize panel properties
    _rect        = ::xmlRect( node, "rect" );
    _textureRect = ::xmlRect( node, "uv" );
    _texture     = ::xmlTexture( node, "texture" );
    _color       = ::xmlColor( node, "color" );

    // read hint text if avaiable
    int hintId = 0;
    if( static_cast<TiXmlElement*>( node )->Attribute( "hintId", &hintId ) )
    {
        assert( Gui::iLanguage->getNumStrings() > unsigned int( hintId ) );
        _hint = Gui::iLanguage->getUnicodeString( unsigned int( hintId ) );
    }

    // initialize controls upon a panel
    TiXmlNode* child = node->FirstChild();
    if( child != NULL ) do 
	  {
        if( child->Type() == TiXmlNode::ELEMENT )
        {
            if( strcmp( child->Value(), "window" ) == 0 )
            {
                insertPanel( new GuiWindow( child ) );
            }
            else if( strcmp( child->Value(), "statictext" ) == 0 )
            {
                insertPanel( new GuiStaticText( child ) );
            }
            else if( strcmp( child->Value(), "button" ) == 0 )
            {
                insertPanel( new GuiButton( child ) );
            }
            else if( strcmp( child->Value(), "slider" ) == 0 )
            {
                insertPanel( new GuiSlider( child ) );
            }
            else if( strcmp( child->Value(), "edit" ) == 0 )
            {
                insertPanel( new GuiEdit( child ) );
            }
        }
        child = child->NextSibling();
    }
    while( child != NULL );
}

void GuiPanel::release(void)
{
    delete this;
}

const char* GuiPanel::getName(void)
{
    return _name.c_str();
}

void GuiPanel::setName(const char* name)
{
    _name = name;
}

const wchar_t* GuiPanel::getHint(void)
{
    return _hint.c_str();
}

void GuiPanel::setHint(const wchar_t* hint)
{
    _hint = hint;
}

gui::IGuiPanel* GuiPanel::getParent(void)
{
    return _parent;
}

void GuiPanel::insertPanel(gui::IGuiPanel* panel)
{
    GuiPanel* p = dynamic_cast<GuiPanel*>( panel ); assert( p );
    if( p->_parent ) p->_parent->removePanel( p );   
    _children.push_back( p );
    p->_parent = this;
}

void GuiPanel::removePanel(gui::IGuiPanel* panel)
{
    GuiPanel* p = dynamic_cast<GuiPanel*>( panel ); assert( p );
    for( GuiPanelI guiPanelI = _children.begin();
                   guiPanelI != _children.end();
                   guiPanelI++ )
    {
        if( *guiPanelI == p )
        {
            _children.erase( guiPanelI );
            p->_parent = NULL;
            break;
        }
    }
}

gui::IGuiPanel* GuiPanel::find(const char* name)
{
    if( strcmp( name, _name.c_str() ) == 0 ) return this;

    gui::IGuiPanel* result = NULL;
    for( GuiPanelI guiPanelI = _children.begin();
                   guiPanelI != _children.end();
                   guiPanelI++ )
    {
        result = (*guiPanelI)->find( name );
        if( result ) return result;
    }

    return NULL;
}

bool GuiPanel::getVisible(void)
{
    return _visible;
}

void GuiPanel::setVisible(bool visible)
{
    _visible = visible;
}

gui::Rect GuiPanel::getRect(void)
{
    return gui::Rect( _rect.left, _rect.top, _rect.right, _rect.bottom );
}

gui::Rect GuiPanel::getScreenRect(void)
{
    RECT scRect = clientToScreen( _rect );
    return gui::Rect( scRect.left, scRect.top, scRect.right, scRect.bottom );
}

void GuiPanel::setRect(const gui::Rect& rect)
{
    _rect.left   = rect.left,
    _rect.top    = rect.top,
    _rect.right  = rect.right,
    _rect.bottom = rect.bottom;
}

engine::ITexture* GuiPanel::getTexture(void)
{
    return _texture;
}

void GuiPanel::setTexture(engine::ITexture* texture)
{
    _texture = dynamic_cast<Texture*>( texture );
}

gui::Rect GuiPanel::getTextureRect(void)
{
    return gui::Rect( 
        _textureRect.left, 
        _textureRect.top, 
        _textureRect.right, 
        _textureRect.bottom 
    );
}

void GuiPanel::setTextureRect(const gui::Rect& rect)
{
    _textureRect.left   = rect.left,
    _textureRect.top    = rect.top,
    _textureRect.right  = rect.right,
    _textureRect.bottom = rect.bottom;
}

Vector4f GuiPanel::getColor(void)
{
    return wrap( _color );
}

void GuiPanel::setColor(const Vector4f& color)
{
    _color = wrap( color );
}

void GuiPanel::setRenderCallback(gui::GuiPanelRenderCallback renderCallback, void* data)
{
    _renderCallback     = renderCallback;
    _renderCallbackData = data;
}

/**
 * typecasting
 */

gui::IGuiWindow* GuiPanel::getWindow(void)
{
    return dynamic_cast<GuiWindow*>( this );
}

gui::IGuiStaticText* GuiPanel::getStaticText(void)
{
    return dynamic_cast<GuiStaticText*>( this );
}

gui::IGuiButton* GuiPanel::getButton(void)
{
    return dynamic_cast<GuiButton*>( this );
}

gui::IGuiEdit* GuiPanel::getEdit(void)
{
    return dynamic_cast<GuiEdit*>( this );
}

gui::IGuiSlider* GuiPanel::getSlider(void)
{
    return dynamic_cast<GuiSlider*>( this );
}

/**
 * module internals
 */

void GuiPanel::render(void)
{
    if( !_visible ) return;

    RECT screenRect = clientToScreen( _rect );
	
    Gui::instance->renderRect( screenRect, _texture, _textureRect, _color );
    onRender();
	
    for( GuiPanelI guiPanelI = _children.begin();
                   guiPanelI != _children.end();
                   guiPanelI++ )
    {
        (*guiPanelI)->render();
    }

    if( _renderCallback )
    {
        _renderCallback( this, _renderCallbackData );
    }
}

RECT GuiPanel::clientToScreen(const RECT& rect)
{
    if( _parent == NULL ) return rect;

    RECT parentScreenRect = _parent->clientToScreen( _parent->_rect );
    RECT screenRect;
    screenRect.left   = parentScreenRect.left + rect.left;
    screenRect.top    = parentScreenRect.top  + rect.top;
    screenRect.right  = parentScreenRect.left + rect.right;
    screenRect.bottom = parentScreenRect.top  + rect.bottom;
    return screenRect;
}

RECT GuiPanel::screenToClient(const RECT& rect)
{
    // make rectangle, describing left-top point of a panel
    RECT temp;
    temp.left = temp.right = temp.top = temp.bottom = 0;

    // determine left-top in screen space
    temp = childToScreen( temp );

    // now just subtract
    RECT result;
    result.left = rect.left - temp.left;
    result.top = rect.top - temp.top;
    result.right = rect.right - temp.left;
    result.bottom = rect.bottom - temp.top;

    return result;
}

RECT GuiPanel::childToScreen(const RECT& rect)
{
    RECT clientRect = clientToScreen( _rect );
    RECT childScreenRect;
    childScreenRect.left    = clientRect.left + rect.left;
    childScreenRect.top     = clientRect.top + rect.top;
    childScreenRect.right   = clientRect.left + rect.right;
    childScreenRect.bottom  = clientRect.top + rect.bottom;
    return childScreenRect;
}

GuiPanel* GuiPanel::getPanelUnderScreenPosition(int x, int y)
{
    if( !_visible ) return NULL;

    RECT screenRect = clientToScreen( _rect );
    
    // is this panel is under the screen position?
    if( screenRect.left <= x && screenRect.right >= x &&
        screenRect.top <= y && screenRect.bottom >= y )
    {
        // if this panel has no children, return it as a result;
        if( _children.size() == 0 ) return this;
        // pass each children from back to front
        if( _children.size() )
        {
            GuiPanelI guiPanelI = _children.end();            
            do
            {
                guiPanelI--;
                GuiPanel* childResult = (*guiPanelI)->getPanelUnderScreenPosition( x, y );
                if( childResult != NULL ) return childResult;
            }
            while( guiPanelI != _children.begin() );
        }
        // return this panel if no children result
        return this;
    }

    // position is away of panel
    return NULL;
}

void GuiPanel::dispatchMessage(gui::Message* message)
{
    if( !_visible ) return;

    onMessage( message );

    for( GuiPanelI guiPanelI = _children.begin();
                   guiPanelI != _children.end();
                   guiPanelI++ )
    {
        (*guiPanelI)->dispatchMessage( message );
    }
}

void GuiPanel::setTopPanel(GuiPanel* panel)
{
    for( GuiPanelI guiPanelI = _children.begin();
                   guiPanelI != _children.end();
                   guiPanelI++ )
    {
        if( (*guiPanelI) == panel )
        {
            _children.erase( guiPanelI );
            _children.push_back( panel );
            return;
        }
    }
}