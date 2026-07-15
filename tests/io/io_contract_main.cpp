auto run_source_contract_tests() -> bool;
auto run_storage_contract_tests() -> bool;

auto main() -> int
{
  if (!run_source_contract_tests())
  {
    return 1;
  }
  if (!run_storage_contract_tests())
  {
    return 2;
  }
  return 0;
}
