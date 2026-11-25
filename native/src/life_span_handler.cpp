// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "life_span_handler.h"

#include "client_handler.h"
#include "jni_util.h"
#include "util.h"

LifeSpanHandler::LifeSpanHandler(JNIEnv* env, jobject handler)
    : handle_(env, handler) {}

// TODO(CEFFX): Expose all parameters.
bool LifeSpanHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    int popup_id,
                                    const CefString& target_url,
                                    const CefString& target_frame_name,
                                    WindowOpenDisposition target_disposition,
                                    bool user_gesture,
                                    const CefPopupFeatures& popupFeatures,
                                    CefWindowInfo& windowInfo,
                                    CefRefPtr<CefClient>& client,
                                    CefBrowserSettings& settings,
                                    CefRefPtr<CefDictionaryValue>& extra_info,
                                    bool* no_javascript_access) {
  if (browser->GetHost()->IsWindowRenderingDisabled()) {
    // Off-screen rendering: a popup is a real popup only when Java hands over the
    // browser object for it (onBeforePopupBrowser). Chromium then creates it
    // windowless, with this client, and keeps window.opener — what sign-in popups
    // (Google, Apple, payment) report back through. Without such an object the
    // popup is cancelled, as before.
    ScopedJNIEnv env;
    if (!env)
      return true;

    ScopedJNIBrowser jparent(env, browser);
    ScopedJNIFrame jopener(env, frame);
    jopener.SetTemporary();
    ScopedJNIString jurl(env, target_url);
    ScopedJNIString jname(env, target_frame_name);
    jobject jpopup = nullptr;

    // disposition: CEF_WOD_NEW_POPUP (5) for window.open with window features, a
    // tab disposition for a target=_blank link; width/height 0 when not asked for.
    JNI_CALL_METHOD(env, handle_, "onBeforePopupBrowser",
                    "(Lcom/techsenger/ceffx/core/browser/CefBrowser;Lcom/techsenger/ceffx/core/browser/"
                    "CefFrame;Ljava/lang/String;Ljava/lang/String;IZII)Lcom/techsenger/ceffx/core/browser/"
                    "CefBrowser;",
                    Object, jpopup, jparent.get(), jopener.get(), jurl.get(),
                    jname.get(), (jint)target_disposition,
                    user_gesture ? JNI_TRUE : JNI_FALSE,
                    (jint)(popupFeatures.widthSet ? popupFeatures.width : 0),
                    (jint)(popupFeatures.heightSet ? popupFeatures.height : 0));

    if (jpopup == nullptr)
      return true;

    windowInfo.SetAsWindowless(kNullWindowHandle);
    // As CefBrowser_N::create does for every browser shown in Java UI.
    windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    settings.background_color = CefColorSetARGB(255, 255, 255, 255);

    // Paired with the popup in OnAfterCreated, which releases the global ref.
    registerJBrowser(env->NewGlobalRef(jpopup));
    env->DeleteLocalRef(jpopup);
    return false;
  }

  ScopedJNIEnv env;
  if (!env)
    return false;

  ScopedJNIBrowser jbrowser(env, browser);
  ScopedJNIFrame jframe(env, frame);
  jframe.SetTemporary();
  ScopedJNIString jtargetUrl(env, target_url);
  ScopedJNIString jtargetFrameName(env, target_frame_name);
  jboolean jreturn = JNI_FALSE;

  JNI_CALL_METHOD(env, handle_, "onBeforePopup",
                  "(Lcom/techsenger/ceffx/core/browser/CefBrowser;Lcom/techsenger/ceffx/core/browser/"
                  "CefFrame;Ljava/lang/String;Ljava/lang/String;)Z",
                  Boolean, jreturn, jbrowser.get(), jframe.get(),
                  jtargetUrl.get(), jtargetFrameName.get());

  return (jreturn != JNI_FALSE);
}

void LifeSpanHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  ScopedJNIEnv env;
  if (!env || jbrowsers_.empty())
    return;

  util::AddCefBrowser(browser);

  jobject jbrowser = jbrowsers_.front();
  jbrowsers_.pop_front();

  CefRefPtr<ClientHandler> client =
      (ClientHandler*)browser->GetHost()->GetClient().get();
  client->OnAfterCreated();

  // Add a reference to |browser| that will be released in
  // LifeSpanHandler::OnBeforeClose.
  if (SetCefForJNIObject(env, jbrowser, browser.get(), "CefBrowser")) {
    JNI_CALL_VOID_METHOD(env, handle_, "onAfterCreated",
                         "(Lcom/techsenger/ceffx/core/browser/CefBrowser;)V", jbrowser);
  }

  // Release the global ref added in CefBrowser_N::create.
  env->DeleteGlobalRef(jbrowser);
}

bool LifeSpanHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  ScopedJNIEnv env;
  if (!env)
    return false;

  ScopedJNIBrowser jbrowser(env, browser);
  jboolean jreturn = JNI_FALSE;

  JNI_CALL_METHOD(env, handle_, "doClose", "(Lcom/techsenger/ceffx/core/browser/CefBrowser;)Z",
                  Boolean, jreturn, jbrowser.get());

  return (jreturn != JNI_FALSE);
}

void LifeSpanHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();
  ScopedJNIEnv env;
  if (!env)
    return;

  ScopedJNIBrowser jbrowser(env, browser);

  JNI_CALL_VOID_METHOD(env, handle_, "onBeforeClose",
                       "(Lcom/techsenger/ceffx/core/browser/CefBrowser;)V", jbrowser.get());

  // Clear the browser pointer member of the Java object. This will
  // release the browser reference that was added in
  // LifeSpanHandler::OnAfterCreated.
  SetCefForJNIObject<CefBrowser>(env, jbrowser, nullptr, "CefBrowser");

  CefRefPtr<ClientHandler> client =
      (ClientHandler*)browser->GetHost()->GetClient().get();
  client->OnBeforeClose(browser);
}

void LifeSpanHandler::OnAfterParentChanged(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();
  ScopedJNIEnv env;
  if (!env)
    return;

  ScopedJNIBrowser jbrowser(env, browser);

  JNI_CALL_VOID_METHOD(env, handle_, "onAfterParentChanged",
                       "(Lcom/techsenger/ceffx/core/browser/CefBrowser;)V", jbrowser.get());
}

void LifeSpanHandler::registerJBrowser(jobject browser) {
  jbrowsers_.push_back(browser);
}

void LifeSpanHandler::unregisterJBrowser(jobject browser) {
  jbrowsers_.remove(browser);
}
