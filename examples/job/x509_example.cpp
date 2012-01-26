#include <saga/saga.hpp>
int main (int argc, char* argv[])
{
  try
  {
  
    saga::context c ("x509");
    
    // The following line points the context to a specific (non-default)
    // user proxy. If you comment out this line, the X.509 context adaptor 
    // will try to pick up your default user proxy, i.e., /tmp/x509_up<UID>
    //c.set_attribute("UserProxy", "/tmp/x509_user_proxy");
    
    saga::session s;
    s.add_context (c);

    saga::job::description jd;
    jd.set_attribute        ("Executable", "/bin/sleep");
    std::vector <std::string> args;
    args.push_back  ("10");
    jd.set_vector_attribute ("Arguments", args );
    
    saga::job::service js (s, "condor://localhost");
    saga::job::job j = js.create_job (jd);

    j.run ();
  }
  catch ( saga::exception const & e )
  {
    std::cerr << e.what ();
  }

  return 0;
}
