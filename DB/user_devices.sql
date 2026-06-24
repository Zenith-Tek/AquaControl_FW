create table public.user_devices (
  id uuid not null default gen_random_uuid (),
  user_id uuid null,
  mac_id text not null,
  device_name text null,
  created_at timestamp with time zone null default now(),
  constraint user_devices_pkey primary key (id),
  constraint user_devices_mac_id_key unique (mac_id),
  constraint user_devices_user_id_fkey foreign KEY (user_id) references auth.users (id) on delete CASCADE
) TABLESPACE pg_default;