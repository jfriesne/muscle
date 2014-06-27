/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "reflector/FilterSessionFactory.h"
#include "reflector/StorageReflectConstants.h"

namespace muscle {

FilterSessionFactory :: FilterSessionFactory(const ReflectSessionFactoryRef & slaveRef, uint32 msph, uint32 tms) : ProxySessionFactory(slaveRef), _tempLogFor(NULL), _maxSessionsPerHost(msph), _totalMaxSessions(tms)
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
      return AbstractReflectSessionRef();
   }
      
   if (_maxSessionsPerHost != MUSCLE_NO_LIMIT)
   {
      uint32 count = 0;
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         if ((iter.GetValue()())&&(strcmp(iter.GetValue()()->GetHostName()(), clientHostIP()) == 0)&&(++count >= _maxSessionsPerHost))
         {
            LogTime(MUSCLE_LOG_DEBUG, "Connection from [%s] refused (host already has " UINT32_FORMAT_SPEC " sessions open).\n", clientHostIP(), _maxSessionsPerHost);
            return AbstractReflectSessionRef();
         }
      }
   }

   AbstractReflectSessionRef ret;
   if (GetSlave()())
   {
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
            return AbstractReflectSessionRef();
         }
      }

      // This IP must *not* match any of our bans!
      for (HashtableIterator<String, StringMatcherRef> iter(_bans); iter.HasData(); iter++)
      {
         if (iter.GetValue()()->Match(clientHostIP()))
         {
            LogTime(MUSCLE_LOG_DEBUG, "Connection from [%s] matches ban pattern [%s], access denied.\n", clientHostIP(), iter.GetKey()());
            return AbstractReflectSessionRef();
         }
      }

      // Okay, he passes.  We'll let our slave create a session for him.
      ret = GetSlave()()->CreateSession(clientHostIP, iap);
      if (ret())
      {
         if (_inputPolicyRef())  ret()->SetInputPolicy(_inputPolicyRef);
         if (_outputPolicyRef()) ret()->SetOutputPolicy(_outputPolicyRef);
      }
   }
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
      for (int b=0; (msg->FindString(PR_NAME_KEYS, b, &s) == B_NO_ERROR); b++)
      {
         switch(msg->what)
         {
            case PR_COMMAND_ADDBANS:        PutBanPattern(*s);                 break;
            case PR_COMMAND_ADDREQUIRES:    PutRequirePattern(*s);             break;
            case PR_COMMAND_REMOVEBANS:     RemoveMatchingBanPatterns(*s);     break;                    
            case PR_COMMAND_REMOVEREQUIRES: RemoveMatchingRequirePatterns(*s); break;                    
         }
      }
      _tempLogFor = NULL;
   }
}
   
status_t FilterSessionFactory :: PutBanPattern(const String & banPattern)
{
   TCHECKPOINT;

   if (_bans.ContainsKey(banPattern)) return B_NO_ERROR;
   StringMatcherRef newMatcherRef(newnothrow StringMatcher(banPattern));
   if (newMatcherRef())
   {
      if (_bans.Put(banPattern, newMatcherRef) == B_NO_ERROR) 
      {
         if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is banning [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), banPattern(), _tempLogFor->GetPort());
         return B_NO_ERROR;
      }
   }
   else WARN_OUT_OF_MEMORY;

   return B_ERROR;
}

status_t FilterSessionFactory :: PutRequirePattern(const String & requirePattern)
{
   TCHECKPOINT;

   if (_requires.ContainsKey(requirePattern)) return B_NO_ERROR;
   StringMatcherRef newMatcherRef(newnothrow StringMatcher(requirePattern));
   if (newMatcherRef())
   {
      if (_requires.Put(requirePattern, newMatcherRef) == B_NO_ERROR) 
      {
         if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is requiring [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), requirePattern(), _tempLogFor->GetPort());
         return B_NO_ERROR;
      }
   }
   else WARN_OUT_OF_MEMORY;

   return B_ERROR;
}

status_t FilterSessionFactory :: RemoveBanPattern(const String & banPattern)
{
   if (_bans.ContainsKey(banPattern))  // don't Remove() here, since then our argument might be a dangling reference during the LogTime() call
   {
      if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is removing ban [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), banPattern(), _tempLogFor->GetPort());
      (void) _bans.Remove(banPattern);
      return B_NO_ERROR;
   }
   return B_ERROR;
}

status_t FilterSessionFactory :: RemoveRequirePattern(const String & requirePattern)
{
   if (_requires.ContainsKey(requirePattern))  // don't Remove() here, since then our argument might be a dangling reference during the LogTime() call
   {
      if (_tempLogFor) LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is removing requirement [%s] on port %u\n", _tempLogFor->GetHostName()(), _tempLogFor->GetSessionIDString()(), requirePattern(), _tempLogFor->GetPort());
      (void) _requires.Remove(requirePattern);
      return B_NO_ERROR;
   }
   return B_ERROR;
}

void FilterSessionFactory :: RemoveMatchingBanPatterns(const String & exp)
{
   StringMatcher sm(exp);
   for (HashtableIterator<String, StringMatcherRef> iter(_bans); iter.HasData(); iter++) if (sm.Match(iter.GetKey()())) RemoveBanPattern(iter.GetKey());
}


void FilterSessionFactory :: RemoveMatchingRequirePatterns(const String & exp)
{
   StringMatcher sm(exp);
   for (HashtableIterator<String, StringMatcherRef> iter(_requires); iter.HasData(); iter++) if (sm.Match(iter.GetKey()())) RemoveRequirePattern(iter.GetKey());
}

}; // end namespace muscle
