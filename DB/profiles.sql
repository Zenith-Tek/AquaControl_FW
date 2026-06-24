create table public.profiles (
  id uuid not null,
  role text not null default 'user'::text,
  "Time" timestamp with time zone null,
  constraint profiles_pkey primary key (id),
  constraint profiles_id_fkey foreign KEY (id) references auth.users (id) on delete CASCADE
) TABLESPACE pg_default;