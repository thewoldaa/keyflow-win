// ---------------------------------------------------------------------------
// The message bridge between the page and the host.
//
// Every message is a JSON object with a "type" field. The page sends intent
// ("set this parameter"); the host applies it to the document and broadcasts a
// fresh snapshot. The page never patches its own copy optimistically, so the
// two sides cannot drift into disagreeing about the document.
//
// This class is deliberately free of WebView2 types. It is the dispatcher, not
// the transport: the host calls HandleMessage with text that arrived from the
// web view, and the bridge calls back with text to send. That separation is
// what lets the bridge be tested without a window, which the tests do.
// ---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/Model.h"

namespace keyflow {

/// What a message handler decided to do.
struct BridgeReply
{
    /// Text to send back to the page. Empty means send nothing.
    std::string reply;

    /// True when the document changed and the page needs a fresh snapshot.
    bool documentChanged = false;
};

/// The host-side services the bridge needs but does not own.
///
/// An interface rather than direct calls so the bridge can be tested with a
/// stub: the tests care about what the bridge asks for, not about ffmpeg or a
/// file dialog.
class BridgeHost
{
public:
    virtual ~BridgeHost() = default;

    /// Open a file picker and return the chosen path, or empty when cancelled.
    virtual std::string pickOpenFile(const std::string& filterName,
                                     const std::string& filterPattern) = 0;

    /// Open a save picker and return the chosen path, or empty when cancelled.
    virtual std::string pickSaveFile(const std::string& filterName,
                                     const std::string& filterPattern,
                                     const std::string& suggestedName) = 0;

    /// The ffmpeg and ffprobe paths, for the import and render messages.
    virtual std::string ffmpegPath() = 0;
    virtual std::string ffprobePath() = 0;

    /// Start a render. The host owns the progress reporting, because it is the
    /// part that has to keep the window responsive.
    virtual void startRender(const std::string& outputPath,
                             const std::string& videoCodec,
                             int quality,
                             bool useBitrate,
                             int bitrateKbps,
                             bool resize,
                             int width,
                             int height) = 0;

    /// Cancel a render in progress.
    virtual void cancelRender() = 0;

    /// Ask the host to quit.
    virtual void requestQuit() = 0;

    /// A message for the status bar, not an error.
    virtual void reportStatus(const std::string& message) = 0;
};

/// Dispatches messages between the page and the document.
///
/// The host owns the document and the sender function; the bridge owns the
/// protocol. Adding a message type is a change here and in the page's script,
/// and never a change to the host's window or WebView2 code.
class Bridge
{
public:
    /// Sends text to the page. Supplied by the host, which is the only part
    /// that knows how to talk to the web view.
    using Sender = std::function<void(const std::string&)>;

    /// The bridge reads and writes this document directly. It is the host's
    /// document, not a copy, so a change here is visible to the rest of the
    /// application.
    Bridge(Project& document, Sender sender, BridgeHost* host = nullptr);

    /// Handle one message from the page.
    ///
    /// Returns true when the document was modified. The host needs this to
    /// decide whether the document is now unsaved: marking it dirty on every
    /// message would make a freshly opened project look modified the moment
    /// the page said hello.
    ///
    /// Never throws: a malformed message produces an error reply, because a
    /// crash here takes the whole editor down and a bad message is not worth
    /// that.
    bool HandleMessage(const std::string& text);

    /// Send the full document to the page.
    void SendDocument();

    /// Send the effect catalogue, so the page can build its browser and its
    /// inspector without carrying its own copy of the list.
    void SendCatalogue();

    /// Send the current playhead, selection and other editor state.
    void SendState();

    /// Send a message of the given type with a message field.
    void SendError(const std::string& message);

    /// Send an arbitrary object to the page. Used by the host for the ready
    /// handshake and by tests.
    void Send(const std::string& jsonText);

    /// True once the page has sent its ready message. Nothing is sent before
    /// that: ExecuteScript calls made during navigation are dropped silently,
    /// and the page would appear to load with no data.
    bool PageReady() const { return _pageReady; }

    /// Number of messages received. For diagnostics and tests.
    int MessagesHandled() const { return _messagesHandled; }

    /// The playhead, in seconds. Set by the page and read by the host.
    double playhead() const { return _playhead; }

    /// The selected layer id, or empty.
    const std::string& selectedLayerId() const { return _selectedLayerId; }

    /// Mark the document dirty. The host uses this when it changes the
    /// document itself, such as after an import.
    void markDirty() { _dirty = true; }
    bool dirty() const { return _dirty; }
    void clearDirty() { _dirty = false; }

    /// Report a render's progress to the page.
    void sendRenderProgress(int frame, int totalFrames, const std::string& stage);
    void sendRenderComplete(const std::string& outputPath, bool success,
                            const std::string& message);

private:
    /// The page script has loaded and can receive messages.
    void HandleReady();

    /// Replace the document. Sent as a full snapshot rather than a diff.
    void HandleReplaceDocument(const json::Value& message);

    // --- document structure ----------------------------------------------

    void HandleAddLayer(const json::Value& message);
    void HandleRemoveLayer(const json::Value& message);
    void HandleMoveLayer(const json::Value& message);
    void HandleSelectLayer(const json::Value& message);
    void HandleSetLayerProperty(const json::Value& message);
    void HandleDuplicateLayer(const json::Value& message);
    void HandleRenameLayer(const json::Value& message);

    // --- effects ----------------------------------------------------------

    void HandleAddEffect(const json::Value& message);
    void HandleRemoveEffect(const json::Value& message);
    void HandleMoveEffect(const json::Value& message);
    void HandleToggleEffect(const json::Value& message);
    void HandleResetEffect(const json::Value& message);

    // --- parameters -------------------------------------------------------

    void HandleSetParameter(const json::Value& message);
    void HandleSetKeyframe(const json::Value& message);
    void HandleRemoveKeyframe(const json::Value& message);
    void HandleMoveKeyframe(const json::Value& message);
    void HandleSetInterpolation(const json::Value& message);
    void HandleSetExpression(const json::Value& message);

    // --- composition ------------------------------------------------------

    void HandleSetComposition(const json::Value& message);
    void HandleSetPlayhead(const json::Value& message);
    void HandleAddComposition(const json::Value& message);

    // --- transform --------------------------------------------------------

    void HandleSetTransform(const json::Value& message);
    void HandleResetTransform(const json::Value& message);

    // --- media and files --------------------------------------------------

    void HandleImportMedia(const json::Value& message);
    void HandleRelinkAsset(const json::Value& message);
    void HandleNewProject(const json::Value& message);
    void HandleOpenProject(const json::Value& message);
    void HandleSaveProject(const json::Value& message);
    void HandleSaveProjectAs(const json::Value& message);
    void HandleExportProject(const json::Value& message);
    void HandleStartRender(const json::Value& message);
    void HandleCancelRender(const json::Value& message);

    // --- misc -------------------------------------------------------------

    void HandleRequestDocument();
    void HandleRequestCatalogue();
    void HandleUndo();
    void HandleRedo();
    void HandleQuit();
    void HandleOpenExternal(const json::Value& message);

    /// Run the handlers and report whether they changed the document.
    ///
    /// Every mutating handler sets `_changed` rather than returning a value,
    /// so a handler cannot forget to report a change: the flag is set at the
    /// point of mutation, where the fact is unambiguous.
    bool Dispatch(const std::string& type, const json::Value& message);

    // --- helpers ----------------------------------------------------------

    /// Find the layer a message names, or report an error and return null.
    Layer* layerFor(const json::Value& message);
    /// Find the effect a message names, or report an error and return null.
    Effect* effectFor(const json::Value& message, Layer* layer);

    /// Resolve the parameter a message names, whether it belongs to an effect
    /// in the stack or to the layer's own transform.
    ///
    /// One resolver rather than a copy in each handler: five handlers each
    /// walking the same name-to-member mapping is five places for the mapping
    /// to fall out of step, and the symptom of a divergence is one control
    /// that works in the inspector and not on the timeline.
    AnimatedValue* resolveParameter(const json::Value& message, Layer* layer);

    /// Apply the spec's defaults to a new effect, so a freshly added effect
    /// looks the way its author intended rather than being all zeros.
    void applyDefaults(Effect& effect, const std::string& specId);

    /// Push an undo entry before a mutation.
    void pushUndo(const std::string& label);

    Project& _document;
    Sender _sender;
    BridgeHost* _host = nullptr;

    bool _pageReady = false;
    bool _changed = false;
    bool _dirty = false;
    int _messagesHandled = 0;

    double _playhead = 0.0;
    std::string _selectedLayerId;
    std::string _selectedEffectId;
    std::string _projectPath;

    /// The undo stack, as whole-document snapshots.
    ///
    /// Snapshots rather than a command log: a command log needs an inverse for
    /// every mutation, and one missing inverse is an undo that corrupts the
    /// document. A project is a few hundred kilobytes, so the memory is not
    /// the constraint — correctness is.
    std::vector<Project> _undoStack;
    std::vector<Project> _redoStack;
    static constexpr std::size_t kMaxUndoDepth = 100;
};

} // namespace keyflow
