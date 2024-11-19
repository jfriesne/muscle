/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/FilterSessionFactory.h"
#include "reflector/StorageReflectConstants.h"

namespace muscle {

FilterSessionFactory :: FilterSessionFactory(const ReflectSessionFactoryRef & slaveRef, uint32 msph, uint32 tms)
   : ProxySessionFactory(slaveRef)
   , _tempLogFor(NULL)
   , _maxSessionsPerHost(msph)
   , _totalMaxSessions(tms)
{
   // empty
}

FilterSessionFactory :: ~FilterSessionFactory()
{
   // empty
}

AbstractReflectSessionRef FilterSessionFactory :: CreateSession(const String & clientHostIP, const IPAddressAndPort & iap)
{
   TCHECKPOINT;

   if (GetSessions().GetNumItems() >= _totalMaxSessions)
   {
      LogTime(MUSCLE_LOG_DEBUG, "Connection from [%s] refused (all " UINT32_FORMAT_SPEC " sessions slots are in use).\n", clientHostIP(), _totalMaxSessions);
      return B_RESOURCE_LIMIT;
   }

   if (_maxSessionsPerHost != MUSCLE_NO_LIMIT)
   {
      uint32 count = 0;
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         if ((iter.GetValue()())&&(strcmp(iter.GetValue()()->GetHostName()(), clientHostIP()) == 0)&&(++count >= _maxSessionsPerHost))
         {
            LogTime(MUSCLE_LOG_DEBUG, "Connection from [%s] refused (host already has " UINT32_FORMAT_SPEC " sessions open).\n", clientHostIP(), _maxSessionsPerHost);
            return B_RESOURCE_LIMIT;
         }
      }
   }

   AbstractReflectSessionRef ret;
   if (GetSlave()() == NULL) return B_BAD_OBJECT;

   // If we have any requires, then this IP must match at least one of them!
   if (_requires.HasItems())
   {
      bool matched = false;
      for (HashtableIterator<String, StringMatcherRef> iter(_requires); iter.HasData(); iter++)
      {
         if (iter.GetValue()()->Match(clientHostIP()))
         {
            matched = true;
            break;
         }
      }
      if (matched == false)
      {
         LogTime(MUSCLE_LOG_DEBUG, "Connection from [%s] does not match any require pattern, access denied.\n", clientHostIP());
         return B_ACCESS_DENIED;
      }
   }

   // This IP must *not* match any of our bans!
   for (HashtableIterator<String, StringMatcherRef> iter(_bans); iter.HasData(); iter++)
   {
      if (iter.GetValue()()->Match(clientHostIP()))
      {
         LogTime(MUSCLE_LOG_DEBUG, "Connection from [%s] matches ban pattern [%s], access denied.\n", clientHostIP(), iter.GetKey()());
         return B_ACCESS_DENIED;
      }
   }

   // Okay, he passes.  We'll let our slave create a session for him.
   ret = GetSlave()()->CreateSession(clientHostIP, iap);
   MRETURN_ON_ERROR(ret);
   if (_inputPolicyRef())  ret()->SetInputPolicy(_inputPolicyRef);
   if (_outputPolicyRef()) ret()->SetOutputPolicy(_outputPolicyRef);
   return ret;
}

void FilterSessionFactory :: MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void *)
{
   TCHECKPOINT;

   const Message * msg = msgRef();
   if (msg)
   {
      _tempLogFor = &from;
      const String * s;
      for (int b=0; (msg->FindString(PR_NAME_KEYS, b, &s).IsOK()); b++)
      {
         switch(msg->what)
         {
            case PR_COMMAND_ADDBANS:        (void) PutBanPattern(*s);                 break;
            case PR_COMMAND_ADDREQUIRES:    (void) PutRequirePattern(*s);             break;
            case PR_COMMAND_REMOVEBANS:            RemoveMatchingBanPatterns(*s);     break;
            case PR_COMMAND_REMOVEREQUIRES:        RemoveMatchingRequirePatterns(*s); break;
            default:
               LogTime(MUSCLE_LOG_WARNING, "FilterSessionFactory " UINT32_FORMAT_SPEC ":  Unhandled message " UINT32_FORMAT_SPEC " from session [%s]\n", GetFactoryID(), msg->what, from.GetSessionDescriptionString()());
            break;
         }
      }
      _tempLogFor = NULL;
   }
}

status_t FilterSessionFactory :: PutBanPattern(const String & banPattern)
{
   TCHECKPOINT;

   if (_bans.ContainsKey(banPattern)) return B_NO_ERROR;

   StringMatcherRef newMatcherRef(new StringMatcher(banPattern));
   MRETURN_ON_ERROR(_bans.Put(banPattern, newMatcherRef));
   if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is banning [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), banPattern(), _tempLogFor->GetPort());
   return B_NO_ERROR;
}

status_t FilterSessionFactory :: PutRequirePattern(const String & requirePattern)
{
   TCHECKPOINT;

   if (_requires.ContainsKey(requirePattern)) return B_NO_ERROR;

   StringMatcherRef newMatcherRef(new StringMatcher(requirePattern));
   MRETURN_ON_ERROR(_requires.Put(requirePattern, newMatcherRef));
   if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is requiring [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), requirePattern(), _tempLogFor->GetPort());
   return B_NO_ERROR;
}

status_t FilterSessionFactory :: RemoveBanPattern(const String & banPattern)
{
   if (_bans.ContainsKey(banPattern))  // don't Remove() here, since then our argument might be a dangling reference during the LogTime() call
   {
      if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is removing ban [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), banPattern(), _tempLogFor->GetPort());
      (void) _bans.Remove(banPattern);
      return B_NO_ERROR;
   }
   else return B_DATA_NOT_FOUND;
}

status_t FilterSessionFactory :: RemoveRequirePattern(const String & requirePattern)
{
   if (_requires.ContainsKey(requirePattern))  // don't Remove() here, since then our argument might be a dangling reference during the LogTime() call
   {
      if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is removing requirement [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), requirePattern(), _tempLogFor->GetPort());
      (void) _requires.Remove(requirePattern);
      return B_NO_ERROR;
   }
   return B_DATA_NOT_FOUND;
}

void FilterSessionFactory :: RemoveMatchingBanPatterns(const String & exp)
{
   StringMatcher sm(exp);
   for (HashtableIterator<String, StringMatcherRef> iter(_bans); iter.HasData(); iter++) if (sm.Match(iter.GetKey()())) (void) RemoveBanPattern(iter.GetKey());
}


void FilterSessionFactory :: RemoveMatchingRequirePatterns(const String & exp)
{
   StringMatcher sm(exp);
   for (HashtableIterator<String, StringMatcherRef> iter(_requires); iter.HasData(); iter++) if (sm.Match(iter.GetKey()())) (void) RemoveRequirePattern(iter.GetKey());
}

} // end namespace muscle
