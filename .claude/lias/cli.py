from __future__ import annotations
import argparse, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from core import add_knowledge, bootstrap, doctor, list_automations, run_automation, search_knowledge

def emit(value: object) -> int:
    print(json.dumps(value, indent=2, sort_keys=True)); return 0

def main() -> int:
    parser = argparse.ArgumentParser(description="Project-local LIAS diagnostics and automation CLI")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("doctor"); sub.add_parser("status"); sub.add_parser("bootstrap")
    knowledge = sub.add_parser("knowledge"); ksub = knowledge.add_subparsers(dest="knowledge_command", required=True)
    add = ksub.add_parser("add"); add.add_argument("category"); add.add_argument("content"); add.add_argument("--source", required=True); add.add_argument("--verified", action="store_true")
    search = ksub.add_parser("search"); search.add_argument("query")
    automation = sub.add_parser("automation"); asub = automation.add_subparsers(dest="automation_command", required=True)
    asub.add_parser("list"); run = asub.add_parser("run"); run.add_argument("id")
    args = parser.parse_args()
    try:
        if args.command == "bootstrap": bootstrap(); return emit({"status":"success","automations":list_automations()})
        if args.command in {"doctor", "status"}: return emit(doctor())
        if args.command == "knowledge" and args.knowledge_command == "add": return emit({"id": add_knowledge(args.category,args.content,args.source,"verified" if args.verified else "unverified")})
        if args.command == "knowledge": return emit(search_knowledge(args.query))
        if args.automation_command == "list": return emit(list_automations())
        return emit(run_automation(args.id))
    except (ValueError, RuntimeError) as exc:
        print(json.dumps({"status":"error","error":str(exc)}), file=sys.stderr); return 2
if __name__ == "__main__": raise SystemExit(main())
