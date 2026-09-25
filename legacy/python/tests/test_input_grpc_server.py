import concurrent.futures
import threading
import time
import unittest

from pas.capture_grpc import GrpcEndpoint
from pas.contracts import TouchCommand
from pas.input_grpc import EmulatorGrpcTouch


class GrpcServerTests(unittest.TestCase):
    def setUp(self):
        try:
            import grpc
            from google.protobuf.empty_pb2 import Empty
            from pas.emulator_proto import emulator_controller_pb2 as pb
        except ImportError:
            self.skipTest("emulator-grpc extra unavailable")
        self.grpc,self.Empty,self.pb=grpc,Empty,pb
        self.received=[]
        self.delay=False
        self.server=grpc.server(concurrent.futures.ThreadPoolExecutor(max_workers=2))
        def handle(request,context):
            self.received.append((request,list(context.invocation_metadata())))
            if self.delay:
                time.sleep(.1)
            return Empty()
        handler=grpc.method_handlers_generic_handler("android.emulation.control.EmulatorController",
            {"sendTouch":grpc.unary_unary_rpc_method_handler(handle,
                request_deserializer=pb.TouchEvent.FromString,
                response_serializer=Empty.SerializeToString)})
        self.server.add_generic_rpc_handlers((handler,))
        port=self.server.add_insecure_port("127.0.0.1:0")
        self.server.start()
        self.endpoint=GrpcEndpoint(f"127.0.0.1:{port}","fixture-secret","fake-server")

    def tearDown(self):
        self.server.stop(1).wait()

    def test_batch_encoding_authorization_and_release(self):
        touch=EmulatorGrpcTouch("fake",720,1280,endpoint=self.endpoint,max_contacts=2)
        down=(TouchCommand("a",0,"down",100,200,1,0),
              TouchCommand("b",1,"down",300,400,1,0))
        self.assertTrue(all(r.success for r in touch.inject_batch(down)))
        self.assertEqual(touch.active_ids,(0,1))
        request,metadata=self.received[0]
        self.assertEqual([(p.identifier,p.x,p.y,p.pressure) for p in request.touches],
                         [(0,100,200,1),(1,300,400,1)])
        self.assertIn(("authorization","Bearer fixture-secret"),[(m.key,m.value) for m in metadata])
        report=touch.release_all()
        self.assertEqual(report.requested_ids,(0,1))
        self.assertFalse(report.failed_ids)
        self.assertEqual(report.effect_unverified_ids,(0,1))
        self.assertTrue(all(event.touches[0].pressure==0 for event,_ in self.received[1:]))
        touch.close()

    def test_timeout_never_retries_down(self):
        touch=EmulatorGrpcTouch("fake",720,1280,endpoint=self.endpoint,timeout_s=.005)
        self.delay=True
        receipt=touch.inject(TouchCommand("a",0,"down",100,200,1,0))
        self.assertFalse(receipt.success)
        self.assertEqual(touch.active_ids,(0,))
        with self.assertRaises(RuntimeError):
            touch.inject(TouchCommand("b",0,"down",100,200,2,0))
        self.delay=False
        report=touch.release_all()
        self.assertEqual(report.requested_ids,(0,))
        self.assertEqual(len([event for event,_ in self.received if event.touches[0].pressure==1]),1)
        with self.assertRaises(RuntimeError):
            touch.inject(TouchCommand("new",0,"down",100,200,3,0))
        touch.close()


if __name__=="__main__": unittest.main()
