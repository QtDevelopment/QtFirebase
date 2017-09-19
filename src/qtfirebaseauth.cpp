#include "qtfirebaseauth.h"

namespace auth = ::firebase::auth;

QtFirebaseAuth *QtFirebaseAuth::self = 0;

QtFirebaseAuth::QtFirebaseAuth(QObject *parent) : QtFirebaseService(parent),
    m_auth(nullptr)
    ,m_complete(false)
    ,m_signedIn(false)
    ,m_errId(AuthErrorNone)
    ,m_action(AuthActionSignIn)
{
    if(self == 0)
    {
        self = this;
        qDebug() << self << "::QtFirebaseAuth" << "singleton";
    }
    startInit();
}

void QtFirebaseAuth::clearError()
{
    setError(AuthErrorNone);
}

void QtFirebaseAuth::setError(int errId, const QString &errMsg)
{
    m_errId = errId;
    m_errMsg = errMsg;
}

void QtFirebaseAuth::registerUser(const QString &email, const QString &pass)
{
    if(running())
        return;

    clearError();
    setComplete(false);
    m_action = AuthActionRegister;
    firebase::Future<auth::User*> future =
           m_auth->CreateUserWithEmailAndPassword(email.toUtf8().constData(), pass.toUtf8().constData());
    qFirebase->addFuture(__QTFIREBASE_ID + QStringLiteral(".auth.register"), future);
}

bool QtFirebaseAuth::signedIn() const
{
    return m_signedIn;
}

void QtFirebaseAuth::signIn(const QString &email, const QString &pass)
{
    if(running())
        return;

    m_action = AuthActionSignIn;
    clearError();
    setComplete(false);

    if(signedIn())
    {
        signOut();
    }
    firebase::Future<auth::User*> future =
                  m_auth->SignInWithEmailAndPassword(email.toUtf8().constData(), pass.toUtf8().constData());

    qFirebase->addFuture(__QTFIREBASE_ID + QStringLiteral(".auth.signin"), future);
}

bool QtFirebaseAuth::running() const
{
    return !m_complete;
}

void QtFirebaseAuth::signOut()
{
    m_action = AuthActionSignOut;
    m_auth->SignOut();
    clearError();
    setComplete(false);
    setSignIn(false);
    setComplete(true);
}

int QtFirebaseAuth::errorId() const
{
    return m_errId;
}

QString QtFirebaseAuth::errorMsg() const
{
    return m_errMsg;
}

QString QtFirebaseAuth::email() const
{
    if(signedIn())
    {
        return m_auth->current_user()->email().c_str();
    }
    return QString();
}

QString QtFirebaseAuth::displayName() const
{
    if(signedIn())
    {
        return m_auth->current_user()->display_name().c_str();
    }
    return QString();
}

bool QtFirebaseAuth::emailVerified() const
{
    if(signedIn())
    {
        return m_auth->current_user()->is_email_verified();
    }
    return false;
}

QString QtFirebaseAuth::photoUrl() const
{
    if(signedIn())
    {
        return m_auth->current_user()->photo_url().c_str();
    }
    return QString();
}

QString QtFirebaseAuth::uid() const
{
    if(signedIn())
    {
        return m_auth->current_user()->uid().c_str();
    }
    return QString();
}

void QtFirebaseAuth::setComplete(bool complete)
{
    if(m_complete!=complete)
    {
        m_complete = complete;
        emit runningChanged();
        if(m_complete)
            emit completed(m_errId == AuthErrorNone, m_action);
    }
}

void QtFirebaseAuth::setSignIn(bool value)
{
    if(m_signedIn!=value)
    {
        m_signedIn = value;
        emit signedInChanged();
    }
}

void QtFirebaseAuth::init()
{
    if(!qFirebase->ready()) {
        qDebug() << self << "::init" << "base not ready";
        return;
    }

    if(!ready() && !initializing()) {
        setInitializing(true);
        m_auth = auth::Auth::GetAuth(qFirebase->firebaseApp());
        qDebug() << self << "::init" << "native initialized";
        setInitializing(false);
        setReady(true);

        auth::User* user = m_auth->current_user();
        if(user!=nullptr)
            setSignIn(true);
        else
            setSignIn(false);
        setComplete(true);
    }
}

void QtFirebaseAuth::onFutureEvent(QString eventId, firebase::FutureBase future)
{
    if(!eventId.startsWith(__QTFIREBASE_ID + QStringLiteral(".auth")))
        return;

    qDebug()<<self<<"::onFutureEvent"<<eventId;

    if(future.status() != firebase::kFutureStatusComplete)
    {
        qDebug() << this << "::onFutureEvent register user failed." << "ERROR: Action failed with error code and message: " << future.error() << future.error_message();
        setError(AuthErrorFailure, "Unknown error");
    }
    else if(future.error()==auth::kAuthErrorNone)
    {
        clearError();
        if(eventId == __QTFIREBASE_ID + QStringLiteral(".auth.register"))
        {
            if(future.result_void() == nullptr)
            {
                setError(AuthErrorFailure, "Registered user is null");
                qDebug()<<"Registered user is null";
            }
            else
            {
                auth::User* user = result<auth::User*>(future.result_void())
                                                 ? *(result<auth::User*>(future.result_void()))
                                                 : nullptr;
                if(user!=nullptr)
                {
                    qFirebase->addFuture(__QTFIREBASE_ID + QStringLiteral(".auth.sendemailverify"), user->SendEmailVerification());
                }
            }
        }
        else if(eventId == __QTFIREBASE_ID + QStringLiteral(".auth.sendemailverify"))
        {
            qDebug() << this << "::onFutureEvent Verification email sent successfully";
        }
        else if(eventId == __QTFIREBASE_ID + QStringLiteral(".auth.signin"))
        {

            qDebug() << this << "::onFutureEvent Sign in successful";
            auth::User* user = result<auth::User*>(future.result_void())
                                             ? *(result<auth::User*>(future.result_void()))
                                             : nullptr;
            if(user!=nullptr)
            {
                setSignIn(true);
                /*qDebug()<<"Email:"<<user->email().c_str();
                qDebug()<<"Display name:"<<user->display_name().c_str();
                qDebug()<<"Photo url:"<<user->photo_url().c_str();
                qDebug()<<"provider_id:"<<user->provider_id().c_str();
                qDebug()<<"is_anonymous:"<<user->is_anonymous();
                qDebug()<<"is_email_verified:"<<user->is_email_verified();*/
            }
        }
    }
    else
    {
        if(eventId == __QTFIREBASE_ID + QStringLiteral(".auth.register"))
        {
            qDebug() << this << "::onFutureEvent Registering user completed with error:"<<future.error()<<future.error_message();
        }
        else if(eventId == __QTFIREBASE_ID + QStringLiteral(".auth.sendemailverify"))
        {
            qDebug() << this << "::onFutureEvent Verification email send error:"<<future.error()<<future.error_message();
        }
        else if(eventId == __QTFIREBASE_ID + QStringLiteral(".auth.signin"))
        {
            setSignIn(false);
            qDebug() << this << "::onFutureEvent Sign in error:"<<future.error()<<future.error_message();
        }
        setError(future.error(), future.error_message());
    }
    setComplete(true);
}
